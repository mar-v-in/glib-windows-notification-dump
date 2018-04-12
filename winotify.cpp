#include "winotify.h"

#include <SDKDDKVer.h>

#include <Windows.h>
#include <sal.h>
#include <Psapi.h>
#include <strsafe.h>
#include <ObjBase.h>
#include <ShObjIdl.h>
#include <propvarutil.h>
#include <functiondiscoverykeys.h>
#include <intsafe.h>
#include <guiddef.h>

#include <roapi.h>
#include <wrl\client.h>
#include <wrl\implements.h>
#include <windows.ui.notifications.h>

#include <string>
#include <codecvt>
#include <comdef.h>

using namespace Microsoft::WRL;
using namespace ABI::Windows::UI::Notifications;
using namespace ABI::Windows::Data::Xml::Dom;
using namespace Windows::Foundation;

typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::UI::Notifications::ToastNotification *, ::IInspectable *> ToastActivatedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::UI::Notifications::ToastNotification *, ABI::Windows::UI::Notifications::ToastDismissedEventArgs *> ToastDismissedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::UI::Notifications::ToastNotification *, ABI::Windows::UI::Notifications::ToastFailedEventArgs *> ToastFailedEventHandler;

class StringReferenceWrapper {
public:

	// Constructor which takes an existing string buffer and its length as the parameters.
	// It fills an HSTRING_HEADER struct with the parameter.      
	// Warning: The caller must ensure the lifetime of the buffer outlives this      
	// object as it does not make a copy of the wide string memory.      

	StringReferenceWrapper(_In_reads_(length) PCWSTR stringRef, _In_ UINT32 length) throw() {
		HRESULT hr = WindowsCreateStringReference(stringRef, length, &_header, &_hstring);

		if (FAILED(hr)) {
			RaiseException(static_cast<DWORD>(STATUS_INVALID_PARAMETER), EXCEPTION_NONCONTINUABLE, 0, nullptr);
		}
	}

	StringReferenceWrapper(std::wstring wstr) throw() : StringReferenceWrapper(wstr.c_str(), wstr.length()) {

	}

	~StringReferenceWrapper() {
		WindowsDeleteString(_hstring);
	}

	template <size_t N>
	StringReferenceWrapper(_In_reads_(N) wchar_t const (&stringRef)[N]) throw() {
		UINT32 length = N - 1;
		HRESULT hr = WindowsCreateStringReference(stringRef, length, &_header, &_hstring);

		if (FAILED(hr)) {
			RaiseException(static_cast<DWORD>(STATUS_INVALID_PARAMETER), EXCEPTION_NONCONTINUABLE, 0, nullptr);
		}
	}

	template <size_t _>
	StringReferenceWrapper(_In_reads_(_) wchar_t(&stringRef)[_]) throw() {
		UINT32 length;
		HRESULT hr = SizeTToUInt32(wcslen(stringRef), &length);

		if (FAILED(hr)) {
			RaiseException(static_cast<DWORD>(STATUS_INVALID_PARAMETER), EXCEPTION_NONCONTINUABLE, 0, nullptr);
		}

		WindowsCreateStringReference(stringRef, length, &_header, &_hstring);
	}

	HSTRING Get() const throw() {
		return _hstring;
	}


private:
	HSTRING             _hstring;
	HSTRING_HEADER      _header;
};

class ToastEventHandler : public Microsoft::WRL::Implements<ToastActivatedEventHandler, ToastDismissedEventHandler, ToastFailedEventHandler> {
public:
	ToastEventHandler::ToastEventHandler(WinotifyToast* toast) {
		this->toast = winotify_toast_ref(toast);
	}
	~ToastEventHandler() {
		winotify_toast_unref(this->toast);
	}

	// ToastActivatedEventHandler 
	IFACEMETHODIMP Invoke(_In_ ABI::Windows::UI::Notifications::IToastNotification *sender, _In_ IInspectable* args) {
		if (toast != nullptr && toast->on_click != nullptr) toast->on_click(toast->on_click_target);
		return S_OK;
	}

	// ToastDismissedEventHandler
	IFACEMETHODIMP Invoke(_In_ ABI::Windows::UI::Notifications::IToastNotification *sender, _In_ ABI::Windows::UI::Notifications::IToastDismissedEventArgs *e) {
		ToastDismissalReason tdr;
		HRESULT hr = e->get_Reason(&tdr);
		if (SUCCEEDED(hr)) {
			if (toast != nullptr && toast->on_dismiss != nullptr) toast->on_dismiss(tdr, toast->on_dismiss_target);
		}
		return hr;
	}

	// ToastFailedEventHandler
	IFACEMETHODIMP Invoke(_In_ ABI::Windows::UI::Notifications::IToastNotification *sender, _In_ ABI::Windows::UI::Notifications::IToastFailedEventArgs *e) {
		HRESULT error = E_FAIL;
		HRESULT hr = e->get_ErrorCode(&error);
		if (SUCCEEDED(hr)) {
			if (toast != nullptr && toast->on_failed != nullptr) toast->on_failed(error, toast->on_failed_target);
		}
		return hr;
	}

	// IUnknown
	IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&_ref); }

	IFACEMETHODIMP_(ULONG) Release() {
		ULONG l = InterlockedDecrement(&_ref);
		if (l == 0) delete this;
		return l;
	}

	IFACEMETHODIMP QueryInterface(_In_ REFIID riid, _COM_Outptr_ void **ppv) {
		if (IsEqualIID(riid, IID_IUnknown))
			*ppv = static_cast<IUnknown*>(static_cast<ToastActivatedEventHandler*>(this));
		else if (IsEqualIID(riid, __uuidof(ToastActivatedEventHandler)))
			*ppv = static_cast<ToastActivatedEventHandler*>(this);
		else if (IsEqualIID(riid, __uuidof(ToastDismissedEventHandler)))
			*ppv = static_cast<ToastDismissedEventHandler*>(this);
		else if (IsEqualIID(riid, __uuidof(ToastFailedEventHandler)))
			*ppv = static_cast<ToastFailedEventHandler*>(this);
		else *ppv = nullptr;

		if (*ppv) {
			reinterpret_cast<IUnknown*>(*ppv)->AddRef();
			return S_OK;
		}

		return E_NOINTERFACE;
	}

private:
	ULONG _ref;
	WinotifyToast* toast;
};

HRESULT winotify_internal_create_toast(_In_ IToastNotificationManagerStatics *toastManager, _In_ HSTRING app_id, _In_ IXmlDocument *xml, WinotifyToast* toaster) {
	ComPtr<IToastNotifier> notifier;
	HRESULT hr = toastManager->CreateToastNotifierWithId(app_id, &notifier);
	if (FAILED(hr)) return hr;
	ComPtr<IToastNotificationFactory> factory;
	hr = GetActivationFactory(StringReferenceWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(), &factory);
	if (FAILED(hr)) return hr;
	ComPtr<IToastNotification> toast;
	hr = factory->CreateToastNotification(xml, &toast);
	if (FAILED(hr)) return hr;

	// Register the event handlers
	EventRegistrationToken activatedToken, dismissedToken, failedToken;
	ComPtr<ToastEventHandler> eventHandler(new ToastEventHandler(toaster));

	hr = toast->add_Activated(eventHandler.Get(), &activatedToken);
	if (FAILED(hr)) return hr;
	hr = toast->add_Dismissed(eventHandler.Get(), &dismissedToken);
	if (FAILED(hr)) return hr;
	hr = toast->add_Failed(eventHandler.Get(), &failedToken);
	if (FAILED(hr)) return hr;
	hr = notifier->Show(toast.Get());
	return hr;
}

HRESULT winotify_internal_create_toast_xml_from_string(_In_ HSTRING toastXmlStr, _Outptr_ IXmlDocument** inputXml) {
	ComPtr<IXmlDocumentIO> toastXml;
	HRESULT hr = Windows::Foundation::ActivateInstance(StringReferenceWrapper(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(), &toastXml);
	if (FAILED(hr)) return hr;
	hr = toastXml->LoadXml(toastXmlStr);
	if (FAILED(hr)) return hr;
	toastXml.CopyTo(inputXml);
	return hr;
}

HRESULT winotify_internal_display_toast_from_xml_string(_In_ HSTRING app_id, _In_ HSTRING toastXmlStr, WinotifyToast* toast) {
	ComPtr<IToastNotificationManagerStatics> toastStatics;
	HRESULT hr = GetActivationFactory(StringReferenceWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(), &toastStatics);
	if (FAILED(hr)) return hr;
	ComPtr<IXmlDocument> toastXml;
	hr = winotify_internal_create_toast_xml_from_string(toastXmlStr, &toastXml);
	if (FAILED(hr)) return hr;
	hr = winotify_internal_create_toast(toastStatics.Get(), app_id, toastXml.Get(), toast);
	return hr;
}

static std::wstring widen(const std::string& to_widen) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(to_widen);
}

static std::string narrow(const std::wstring& to_narrow) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.to_bytes(to_narrow);
}

HSTRING winotify_internal_create_hstring(const char* cstr) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wstr = converter.from_bytes(cstr);
	HSTRING text;
	WindowsCreateString(wstr.c_str(), wstr.length(), &text);
	return text;
}

static bool initialized = false;

WINOTIFY_EXPORT WinotifyToast* winotify_toast_new() {
	if (!initialized) {
		HRESULT res = Initialize(RO_INIT_SINGLETHREADED);
		if (FAILED(res)) {
			_com_error err(res);
			LPCTSTR errMsg = err.ErrorMessage();
			printf("Error initializing Windows runtime: %s\n", errMsg);
			return nullptr;
		}
		initialized = true;
	}
	WinotifyToast* toast = (WinotifyToast*) malloc(sizeof(WinotifyToast));
	memset(toast, 0, sizeof(WinotifyToast));
	toast->app_id = nullptr;
	toast->ref_count = 1;
	return toast;
}

WINOTIFY_EXPORT WinotifyToast* winotify_toast_destroy(WinotifyToast* toast) {
	//if (toast->app_id != nullptr) free(toast->app_id);
	toast->app_id = nullptr;
	//if (toast->xml != nullptr) free(toast->xml);
	toast->xml = nullptr;
	if (toast->on_click_target_destroy_notify != nullptr) toast->on_click_target_destroy_notify(toast->on_click_target);
	toast->on_click = nullptr;
	toast->on_click_target = nullptr;
	toast->on_click_target_destroy_notify = nullptr;
	if (toast->on_dismiss_target_destroy_notify != nullptr) toast->on_dismiss_target_destroy_notify(toast->on_dismiss_target);
	toast->on_dismiss = nullptr;
	toast->on_dismiss_target = nullptr;
	toast->on_dismiss_target_destroy_notify = nullptr;
	if (toast->on_failed_target_destroy_notify != nullptr) toast->on_failed_target_destroy_notify(toast->on_failed_target);
	toast->on_failed = nullptr;
	toast->on_failed_target = nullptr;
	toast->on_failed_target_destroy_notify = nullptr;
	//free(toast);
	return nullptr;
}

WINOTIFY_EXPORT WinotifyToast* winotify_toast_ref(WinotifyToast* toast) {
	if (toast->ref_count > 0) {
		toast->ref_count++;
		return toast;
	} else {
		return nullptr;
	}
}

WINOTIFY_EXPORT WinotifyToast* winotify_toast_unref(WinotifyToast* toast) {
	if (toast->ref_count > 0) {
		toast->ref_count--;
		if (toast->ref_count == 0) {
			winotify_toast_destroy(toast);
			return nullptr;
		}
		return toast;
	} else {
		return nullptr;
	}
}

WINOTIFY_EXPORT void winotify_toast_display(WinotifyToast* toast) {
	winotify_internal_display_toast_from_xml_string(winotify_internal_create_hstring(toast->app_id), winotify_internal_create_hstring(toast->xml), toast);
}