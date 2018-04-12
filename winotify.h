#pragma once

#ifndef WINOTIFY_EXPORT
#define WINOTIFY_EXPORT  extern "C" __declspec(dllexport)
#endif // !WINOTIFY_EXPORT

typedef void(*WinotifyToastClickListener) (void* user_data);
typedef void(*WinotifyToastDismissListener) (unsigned int reason, void* user_data);
typedef void(*WinotifyToastFailedListener) (unsigned int error_code, void* user_data);
typedef void(*WinotifyDestroyNotify) (void* user_data);

typedef struct WinotifyToast {
	volatile int ref_count;

	char* app_id;
	char* xml;

	WinotifyToastClickListener on_click;
	void* on_click_target;
	WinotifyDestroyNotify on_click_target_destroy_notify;
	WinotifyToastDismissListener on_dismiss;
	void* on_dismiss_target;
	WinotifyDestroyNotify on_dismiss_target_destroy_notify;
	WinotifyToastFailedListener on_failed;
	void* on_failed_target;
	WinotifyDestroyNotify on_failed_target_destroy_notify;
} WinotifyToast;

WINOTIFY_EXPORT WinotifyToast* winotify_toast_new();
WINOTIFY_EXPORT WinotifyToast* winotify_toast_destroy(WinotifyToast* self);
WINOTIFY_EXPORT WinotifyToast* winotify_toast_ref(WinotifyToast* self);
WINOTIFY_EXPORT WinotifyToast* winotify_toast_unref(WinotifyToast* self);
WINOTIFY_EXPORT void winotify_toast_display(WinotifyToast* self);