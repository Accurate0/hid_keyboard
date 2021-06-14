#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <hidapi.h>
#include <libusb.h>

// TODO: send current volume first, before callback register
// TODO: handle disconnects
// TODO: linux
// TODO: add more cool stuff

#define __MINGW32__

#ifdef __MINGW32__
    #include <utility>

    #include <Windows.h>
    #include <mmdeviceapi.h>
    #include <endpointvolume.h>
    #include <audioclient.h>

    template<typename T>
    class CAudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback
    {
        LONG _cRef;
        T m_callback;

    public:
        CAudioEndpointVolumeCallback(T callback) :
            _cRef(1), m_callback(std::move(callback))
        {
        }

        // IUnknown methods -- AddRef, Release, and QueryInterface
        ULONG STDMETHODCALLTYPE AddRef()
        {
            return InterlockedIncrement(&_cRef);
        }

        ULONG STDMETHODCALLTYPE Release()
        {
            ULONG ulRef = InterlockedDecrement(&_cRef);
            if (ulRef == 0)
            {
                delete this;
            }
            return ulRef;
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface)
        {
            if (IID_IUnknown == riid)
            {
                AddRef();
                *ppvInterface = (IUnknown*)this;
            }
            else if (__uuidof(IAudioEndpointVolumeCallback) == riid)
            {
                AddRef();
                *ppvInterface = (IAudioEndpointVolumeCallback*)this;
            }
            else
            {
                *ppvInterface = NULL;
                return E_NOINTERFACE;
            }
            return S_OK;
        }

        // Callback method for endpoint-volume-change notifications.

        HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify)
        {
            m_callback(pNotify->fMasterVolume);
            return S_OK;
        }
    };

    void VolumeLoop(IAudioEndpointVolumeCallback *callback)
    {
#define ON_ERROR(hr) \
    if(hr) { goto END; }

        HRESULT hr;
        IMMDeviceEnumerator* pDeviceEnumerator = nullptr;
        IMMDevice* pDevice = nullptr;
        IAudioEndpointVolume* pAudioEndpointVolume = nullptr;

        CoInitialize(nullptr);
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pDeviceEnumerator);
        ON_ERROR(hr);

        hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        ON_ERROR(hr);

        hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pAudioEndpointVolume);
        ON_ERROR(hr);

        pAudioEndpointVolume->RegisterControlChangeNotify(callback);

        return;
#undef ON_ERROR
END:
        if(pAudioEndpointVolume) {
            pAudioEndpointVolume->Release();
            pAudioEndpointVolume->UnregisterControlChangeNotify(callback);
        }
        if(pDevice)
            pDevice->Release();
        if(pDeviceEnumerator)
            pDeviceEnumerator->Release();

        CoUninitialize();
    }
#endif

class HID {
public:
    static HID hid;
    HID() {
        if(hid_init())
            abort();
    }

    ~HID() {
        hid_exit();
    }

    class Device {
    private:
        hid_device *m_handle { nullptr };

        Device(hid_device *handle) : m_handle(handle)
        {
        }

    public:
        static std::optional<Device> construct(uint16_t vid, uint16_t pid, uint16_t usage, uint16_t usage_page) {
            struct hid_device_info *devs = hid_enumerate(vid, pid);
            struct hid_device_info *cur_dev = devs;
            const char *path { nullptr };
            while(cur_dev) {
                if(cur_dev->usage == usage && cur_dev->usage_page == usage_page) {
                    path = cur_dev->path;
                    break;
                }

                cur_dev = cur_dev->next;
            }

            if(!path)
                return {};

            printf("open path -> %s\n", path);
            hid_device *handle = hid_open_path(path);
            if(!handle)
                return {};

            wchar_t product[32];
            hid_get_product_string(handle, product, 32);
            printf("connected to %S\n", product);

            hid_free_enumeration(devs);

            return std::make_optional(HID::Device(handle));
        }

        // ~Device() {
        //     hid_close(m_handle);
        // }

        int write(uint8_t *buf, int size) {
            return hid_write(m_handle, buf, size);
        }

        int read(uint8_t *buf, int size) {
            return hid_read(m_handle, buf, size);
        }

        const wchar_t* error() {
            return hid_error(m_handle);
        }
    };
};


#define VENDOR_ID       0x320F
#define PRODUCT_ID      0x5044
#define USAGE           0x61
#define USAGE_PAGE      0xFF60

int main(void)
{
    // WINDOWS NO HOTPLUG SUPPORT YEP
    // https://github.com/libusb/libusb/issues/86
#ifndef __MINGW32__
    libusb_hotplug_callback_handle callback_handle;
    libusb_init(nullptr);

    auto libusb_callback = [](struct libusb_context *ctx,
                              struct libusb_device *dev,
                              libusb_hotplug_event event,
                              void *user_data) -> int
                              {
                                if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
                                    printf("Arrived\n");
                                } else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
                                    printf("Left\n");
                                } else {
                                    printf("Unhandled event %d\n", event);
                                }

                                return 0;
                              };

    int rc = libusb_hotplug_register_callback(nullptr,
                                            LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                            0, VENDOR_ID, PRODUCT_ID, LIBUSB_HOTPLUG_MATCH_ANY, libusb_callback, nullptr, &callback_handle);

    printf("%d\n", libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG ));
#endif

    auto kb_or_null = HID::Device::construct(VENDOR_ID, PRODUCT_ID, USAGE, USAGE_PAGE);
    // auto keyboard = dev_or_fail.value();
    if(!kb_or_null.has_value()) {
        std::cerr << "no kb detected\n";
    }

#ifdef __MINGW32__
    CAudioEndpointVolumeCallback callback = [&](float volume) {
        uint8_t buf[32] = { 0 };
        // wtf happens to buf[0]????
        buf[1] = 0x1;
        buf[2] = static_cast<uint8_t>(volume * 100);
        if(kb_or_null.has_value()) {
            kb_or_null->write(buf, 32);
            printf("cmd: %d data: %d -> %S\n", buf[1], buf[2], kb_or_null->error());
        }
    };

    VolumeLoop(&callback);
#endif

    while(true) {
#ifndef __MINGW32__
        libusb_handle_events_completed(nullptr, nullptr);
#endif
        Sleep(1000);
    }
    return 0;
}
