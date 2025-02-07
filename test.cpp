#define _WIN32_DCOM
#include<iostream>
#include<windows.h>
#include<pdh.h>
#include <pdhmsg.h>
#include <comdef.h>
#include <Wbemidl.h>
using namespace std;
//加载PDH库
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib,"pdh.lib")
int GetCpuUsage();
int GetCpuTemperature();
int main()
//循环出现使用率和温度，间隔一秒  
//1.输出使用率 2.输出温度 3.休眠1000ms
{
	while (1)
	{
		//if正常运行
		if (GetCpuUsage() >= 0 && GetCpuTemperature() >= 0)
		{

			GetCpuUsage();

			GetCpuTemperature();
		}


		if (GetCpuUsage() >= 0 && GetCpuTemperature() < 0)
		{
			GetCpuUsage();
			cout << "Failed to get CPU temperature." << endl;
		}
		if (GetCpuUsage() < 0 && GetCpuTemperature() < 0)
		{
			cout << "Failed to get CPU usage and temperature." << endl;

		}
	


		Sleep(1000);
	}

}
//1.先创建一个查询
//2.然后再向查询中添加一个计数器
//3.收集两次性能数据
int GetCpuUsage()
{
	//1.创建查询 函数PdhOpenQuery
	PDH_HQUERY query;//表示一个性能数据查询会话。
	PDH_STATUS status = PdhOpenQuery(NULL, 0, &query);//status表示状态 1.NULL表示实时数据2.一般为0
	if (status != ERROR_SUCCESS)
	{
		cout << "Error open query" << endl;
		return -1;
	}
	//2.向查询添加计数器 函数PdhAddCounter
	PDH_HCOUNTER counter;//创建一个计数器
	status = PdhAddCounter(query, L"\\Processor(_Total)\\% Processor Time", 0, &counter);
	if (status !=ERROR_SUCCESS) {//没成功
		PdhCloseQuery(query);////关闭查询
		cerr << "Error add counter" << endl;
		return -1;
	}
	//3.收集性能数据  像使用率这样的计数器，需要收集两份样本，中间用Sleep()函数间隔1s 或者更久
	//函数PdhCollectQueryData
	PdhCollectQueryData(query);
	Sleep(1000);
	PdhCollectQueryData(query);
	if (status != ERROR_SUCCESS) {//没成功
		PdhCloseQuery(query);////关闭查询
		cerr << "Error collect query data" << endl;
		return -1;
	}

	//4.显示性能数据 函数PdhGetFormattedCounterValue
	DWORD counterType;
	PDH_FMT_COUNTERVALUE displayValue;
	status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, &counterType, &displayValue);
	//参数1.性能计数器
	// 2.返回值的格式 PDH_FMT_DOUBLE: 返回一个双精度浮点数。
	// 3.接收计数器类型
	// 4.接收计数器值的 PDH_FMT_COUNTERVALUE 结构。用于存储格式化后的性能计数器值。
	//这个函数可以将性能计数器的原始值转换为更易于阅读的格式
	if (status != ERROR_SUCCESS) 
	{
		PdhRemoveCounter(counter);
		PdhCloseQuery(query);
		cerr << "Error get formatted counter value: " << endl;
		return -1;
	}
	else
	{
		cout << "Cpu usage:" << displayValue.doubleValue << "%"<<endl;
	}
	//5.关闭查询
	PdhRemoveCounter(counter);
	PdhCloseQuery(query);
	return 1;
}
int GetCpuTemperature()
{
	//1通过调用 CoInitialize 来初始化 COM 参数。
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		cout << "Failed initialize COM" << endl;
		return -1;
	}
	//2通过调用 CoInitializeSecurity 来初始化 COM 进程安全性。
	hr=CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
		RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
	if (FAILED(hr))
	{
		cout << "Failed initialize security" << endl;
		CoUninitialize();
		return -1;

	}
	//3通过调用 CoCreateInstance 获取 WMI 的初始定位符。
	IWbemLocator* pLocator;
	hr=CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLocator);
	if (FAILED(hr))
	{
		cout << "Failed create instance" << endl;
		CoUninitialize();
		return -1;
	}
	//4通过调用 IWbemLocator::ConnectServer，获取指向本地计算机上 root\cimv2 命名空间的 IWbemServices 的指针。
	IWbemServices* pServices;
	BSTR ns = SysAllocString(L"root\\WMI");
	hr = pLocator->ConnectServer(ns, NULL, NULL, NULL, 0, NULL, NULL, &pServices);
	pLocator->Release();
	if (FAILED(hr))
	{
		cout << "Failed connect to WMI" << endl;
		CoUninitialize();
		return -1;
	}
	// 执行WQL查询，获取温度数据
	   //1. 定义查询语句
	BSTR query = SysAllocString(L"SELECT * FROM MSAcpi_ThermalZoneTemperature");
	BSTR wql = SysAllocString(L"WQL");
	   //2.执行查询
	IEnumWbemClassObject* pEum;
	hr = pServices->ExecQuery(wql, query, WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY, NULL, &pEum);
	SysFreeString(wql);
	SysFreeString(query);
	if (FAILED(hr))
	{
		cout << "Failed query"<< endl;
		pServices->Release();
		CoUninitialize;
		return -1;
	}
	// 获取查询结果
	IWbemClassObject* pObject;
	ULONG returned;
	hr=pEum->Next(WBEM_INFINITE, 1, &pObject, &returned);
	pEum->Release();
	pServices->Release();
	if (FAILED(hr))
	{
		cout << "No result" << endl;
		CoUninitialize;
		return -1;
	}


	BSTR temp = SysAllocString(L"CurrentTemperature");
	VARIANT v;
	VariantInit(&v);
	hr = pObject->Get(temp, 0, &v, NULL, NULL);
	pObject->Release();
	SysFreeString(temp);
	if (FAILED(hr)) {
		cout << "Failed variant temperature" << endl;
		VariantClear(&v);
		CoUninitialize();
		return hr;
	}
	LONG Temperature=V_I4(&v);
	VariantClear(&v);

	// 结束COM
	CoUninitialize();
	double tempCelsius = (double)(Temperature - 2732) / 10.0;
	printf("CPU Temperature: %.2lf°C\n", tempCelsius);
	return 1;

}