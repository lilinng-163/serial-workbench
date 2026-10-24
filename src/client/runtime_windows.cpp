// SPDX-License-Identifier: Apache-2.0
#include "runtime.h"
#include <windows.h>
#include <wbemidl.h>
#include <stdexcept>

namespace sw
{
namespace
{
template<class T> struct ComOwner
{
    T *value = nullptr;
    ~ComOwner() { if (value) { value->Release(); } }
    T **out() { return &value; }
    T *operator->() const { return value; }
};

void checked(HRESULT result)
{
    if (FAILED(result))
    {
        throw std::runtime_error("Windows could not start an independent runtime. Run SerialWorkbenchCLI start from your terminal first.");
    }
}

// Windows command-line quoting, including trailing slashes and embedded quotes.
QString quoted(const QString &argument)
{
    QString result = "\"";
    int slashes = 0;
    for (const auto character : argument)
    {
        if (character == '\\') { ++slashes; continue; }
        if (character == '"')
        {
            result += QString(slashes * 2 + 1, '\\') + character;
        }
        else { result += QString(slashes, '\\') + character; }
        slashes = 0;
    }
    return result + QString(slashes * 2, '\\') + '"';
}

void putString(IWbemClassObject *object, const wchar_t *key, const QString &text)
{
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BSTR;
    value.bstrVal = SysAllocString(reinterpret_cast<const OLECHAR *>(text.utf16()));
    const auto result = object->Put(key, 0, &value, 0);
    VariantClear(&value);
    checked(result);
}
}

bool needsIndependentRuntime()
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info {};
    return QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation, &info, sizeof(info), nullptr)
        && (info.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE);
}

void startIndependentRuntime(const QString &executable, const QStringList &arguments)
{
    // MCP hosts can own a kill-on-close Windows Job. A shared runtime belongs
    // to the user session, not to one Agent's process tree. WMI Process.Create
    // is the documented Windows launch API that does not inherit that job.
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (initialized != RPC_E_CHANGED_MODE) { checked(initialized); }
    const auto cleanup = qScopeGuard([initialized] { if (SUCCEEDED(initialized)) { CoUninitialize(); } });
    ComOwner<IWbemLocator> locator;
    checked(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, reinterpret_cast<void **>(locator.out())));
    ComOwner<IWbemServices> service;
    auto name = SysAllocString(L"ROOT\\CIMV2");
    const auto connected = locator->ConnectServer(name, nullptr, nullptr, nullptr, 0, nullptr, nullptr, service.out());
    SysFreeString(name);
    checked(connected);
    checked(CoSetProxyBlanket(service.value, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE));

    ComOwner<IWbemClassObject> processClass;
    name = SysAllocString(L"Win32_Process");
    const auto gotClass = service->GetObject(name, 0, nullptr, processClass.out(), nullptr);
    SysFreeString(name);
    checked(gotClass);
    ComOwner<IWbemClassObject> definition;
    checked(processClass->GetMethod(L"Create", 0, definition.out(), nullptr));
    ComOwner<IWbemClassObject> input;
    checked(definition->SpawnInstance(0, input.out()));
    QString command = quoted(QDir::toNativeSeparators(executable));
    for (const auto &argument : arguments) { command += ' ' + quoted(argument); }
    putString(input.value, L"CommandLine", command);
    putString(input.value, L"CurrentDirectory", QFileInfo(executable).absolutePath());

    ComOwner<IWbemClassObject> output;
    name = SysAllocString(L"Win32_Process");
    auto method = SysAllocString(L"Create");
    const auto created = service->ExecMethod(name, method, 0, nullptr, input.value, output.out(), nullptr);
    SysFreeString(method);
    SysFreeString(name);
    checked(created);
    VARIANT result;
    VariantInit(&result);
    const auto gotResult = output->Get(L"ReturnValue", 0, &result, nullptr, nullptr);
    const bool success = SUCCEEDED(gotResult) && result.vt == VT_I4 && result.lVal == 0;
    VariantClear(&result);
    if (!success) { checked(E_FAIL); }
}
}
