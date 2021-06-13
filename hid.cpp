#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <hidapi.h>

#define WIN32

#ifdef WIN32
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

        ~CAudioEndpointVolumeCallback()
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
            if (0 == ulRef)
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

        // block
        while(1)
            Sleep(1000);
#undef ON_ERROR
END:
        if(pAudioEndpointVolume)
            pAudioEndpointVolume->Release();
        if(pDevice)
            pDevice->Release();
        if(pDeviceEnumerator)
            pDeviceEnumerator->Release();

        CoUninitialize();
    }
#endif

#define UNUSED(x)       static_cast<void>(x)
#define VENDOR_ID       0x320F
#define PRODUCT_ID      0x5044

class HID {
public:
    HID() {
        if(hid_init())
            abort();
    }

    ~HID() {
        hid_exit();
    }

    class Device {
    private:
    public:
        hid_device *handle { nullptr };
        Device(uint16_t vid, uint16_t pid, uint16_t usage, uint16_t usage_page) {
            // handle = hid_open(vid, pid, NULL);
            // if(!handle)
            //     abort();

            // wchar_t product[32];
            // hid_get_product_string(handle, product, 32);
            // printf("connected to %S\n", product);
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
                abort();

            printf("open path -> %s\n", path);
            handle = hid_open_path(path);
            if(!handle)
                abort();

            wchar_t product[32];
            hid_get_product_string(handle, product, 32);
            printf("connected to %S\n", product);

            hid_free_enumeration(devs);
        }

        ~Device() {
            hid_close(handle);
        }

        int write(uint8_t *buf, int size) {
            return hid_write(handle, buf, size);
        }

        int read(uint8_t *buf, int size) {
            return hid_read(handle, buf, size);
        }
    };
};

static HID hid;

int main(void)
{
    uint8_t buf[32] = { 0 };
    int i;
    HID::Device dev { VENDOR_ID, PRODUCT_ID, 0x61, 0xFF60 };

#ifdef WIN32
    CAudioEndpointVolumeCallback callback = [&](float volume) {
        // wtf happens to buf[0]????
        buf[1] = 0x1;
        buf[2] = static_cast<uint8_t>(volume * 100);
        dev.write(buf, 32);
        printf("cmd: %d data: %d -> %S\n", buf[1], buf[2], hid_error(dev.handle));
    };

    VolumeLoop(&callback);
#endif
    return 0;
}
