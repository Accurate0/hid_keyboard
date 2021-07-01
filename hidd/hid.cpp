#include <hidapi.h>
#include <libusb.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>

#if defined(__linux__)
#include <alsa/asoundlib.h>

#include <cmath>
#include <thread>
#endif

// TODO: send current volume first, before callback register
// TODO: add more cool stuff
// TODO: handle mute events :) switch colour to red ?

#if defined(__MINGW32__)
#include <Windows.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

#include <utility>

template <typename T> class CAudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback {
    LONG _cRef;
    T m_callback;

  public:
    CAudioEndpointVolumeCallback(T callback) : _cRef(1), m_callback(std::move(callback)) {
    }

    // IUnknown methods -- AddRef, Release, and QueryInterface
    ULONG STDMETHODCALLTYPE AddRef() {
        return InterlockedIncrement(&_cRef);
    }

    ULONG STDMETHODCALLTYPE Release() {
        ULONG ulRef = InterlockedDecrement(&_cRef);
        if (ulRef == 0) {
            delete this;
        }
        return ulRef;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface) {
        if (IID_IUnknown == riid) {
            AddRef();
            *ppvInterface = (IUnknown *)this;
        } else if (__uuidof(IAudioEndpointVolumeCallback) == riid) {
            AddRef();
            *ppvInterface = (IAudioEndpointVolumeCallback *)this;
        } else {
            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    // Callback method for endpoint-volume-change notifications.

    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) {
        m_callback(pNotify->fMasterVolume);
        return S_OK;
    }
};
void SetupVolumeCallback(IAudioEndpointVolumeCallback *callback) {
#define ON_ERROR(hr) \
    if (hr) {        \
        goto END;    \
    }

    HRESULT hr;
    IMMDeviceEnumerator *pDeviceEnumerator = nullptr;
    IMMDevice *pDevice = nullptr;
    IAudioEndpointVolume *pAudioEndpointVolume = nullptr;

    CoInitialize(nullptr);
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void **)&pDeviceEnumerator);
    ON_ERROR(hr);

    hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    ON_ERROR(hr);

    hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,
                           (void **)&pAudioEndpointVolume);
    ON_ERROR(hr);

    pAudioEndpointVolume->RegisterControlChangeNotify(callback);

    return;
#undef ON_ERROR
END:
    if (pAudioEndpointVolume) {
        pAudioEndpointVolume->Release();
        pAudioEndpointVolume->UnregisterControlChangeNotify(callback);
    }
    if (pDevice)
        pDevice->Release();
    if (pDeviceEnumerator)
        pDeviceEnumerator->Release();

    CoUninitialize();
}
#endif

#ifdef __linux__
static const char *mix_name = "Master";
static const char *card = "hw:1";
static int mix_index = 0;

uint8_t get_volume() {
    int ret = 0;
    snd_mixer_t *handle = nullptr;
    snd_mixer_selem_id_t *sid = nullptr;
    snd_mixer_elem_t *elem = nullptr;

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, mix_index);
    snd_mixer_selem_id_set_name(sid, mix_name);

    // HOPE NOTHING FAILS HEY
    if ((snd_mixer_open(&handle, 0)) < 0)
        return -1;
    if ((snd_mixer_attach(handle, card)) < 0) {
        snd_mixer_close(handle);
        return -2;
    }
    if ((snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
        snd_mixer_close(handle);
        return -3;
    }
    ret = snd_mixer_load(handle);
    if (ret < 0) {
        snd_mixer_close(handle);
        return -4;
    }
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
        snd_mixer_close(handle);
        return -5;
    }

    // https://github.com/alsa-project/alsa-utils/blob/7a7e064f83f128e4e115c24ef15ba6788b1709a6/alsamixer/volume_mapping.c
    // thanks :)
    long minv, maxv, outvol;
    snd_mixer_selem_get_playback_dB_range(elem, &minv, &maxv);
    if (snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_UNKNOWN, &outvol) < 0) {
        snd_mixer_close(handle);
        return -6;
    }

    double normalised = pow(10, (outvol - maxv) / 6000.0);
    double min_norm = pow(10, (minv - maxv) / 6000.0);
    normalised = (normalised - min_norm) / (1 - min_norm);

    return static_cast<uint8_t>(round(normalised * 100));
}
#else
uint8_t get_volume() { return 0; }
#endif

#define VOLUME_COMMAND 0x1
#define UNUSED(x)      static_cast<void>(x)
#define VENDOR_ID      0x320F
#define PRODUCT_ID     0x5044
#define USAGE          0x61
#define USAGE_PAGE     0xFF60

class HID {
  public:
    static HID hid;
    HID() {
        if (hid_init())
            abort();
    }

    ~HID() {
        hid_exit();
    }

    class Device {
      private:
        hid_device *m_handle{nullptr};

      public:
        Device(hid_device *handle) : m_handle(handle) {
        }

        static hid_device *construct_handle(uint16_t vid, uint16_t pid, uint16_t usage,
                                            uint16_t usage_page) {
            struct hid_device_info *devs = hid_enumerate(vid, pid);
            struct hid_device_info *cur_dev = devs;
            const char *path{nullptr};
            while (cur_dev) {
                if (cur_dev->usage == usage && cur_dev->usage_page == usage_page) {
                    path = cur_dev->path;
                    break;
                }

                cur_dev = cur_dev->next;
            }

            if (!path)
                return nullptr;

            hid_device *handle = hid_open_path(path);
            if (!handle)
                return nullptr;

            hid_free_enumeration(devs);

            return handle;
        }

        static std::optional<Device> construct(uint16_t vid, uint16_t pid, uint16_t usage,
                                               uint16_t usage_page) {
            auto handle = construct_handle(vid, pid, usage, usage_page);
            if (handle)
                return std::make_optional(HID::Device(handle));
            else
                return {};
        }

        // move constructor to avoid destructor being called on return
        Device(Device &&dev) : m_handle(dev.m_handle) {
            dev.m_handle = nullptr;
        }

        ~Device() {
            hid_close(m_handle);
        }

        void print_product() {
            wchar_t product[32];
            hid_get_product_string(m_handle, product, 32);
            std::wcout << "connected to " << product << "\n";
        }

        int write(uint8_t *buf, int size) {
            return hid_write(m_handle, buf, size);
        }

        int read(uint8_t *buf, int size) {
            return hid_read(m_handle, buf, size);
        }

        const wchar_t *error() {
            return hid_error(m_handle);
        }
    };
};

void send_volume(uint8_t volume, std::optional<HID::Device> &kb_or_null) {
    uint8_t buf[32] = {0};
    // wtf happens to buf[0]????
    buf[1] = VOLUME_COMMAND;
    buf[2] = volume;
    if (kb_or_null.has_value()) {
        kb_or_null->write(buf, 32);
        fprintf(stderr, "cmd: %d data: %u -> %S\n", buf[1], buf[2], kb_or_null->error());
    }
}

int main(void) {
    libusb_init(nullptr);
    // lets try to connect as soon as we start
    auto kb_or_null = HID::Device::construct(VENDOR_ID, PRODUCT_ID, USAGE, USAGE_PAGE);
    // doesn't matter if we fail, on linux we setup a hotplug handler
    if (!kb_or_null.has_value())
        std::cerr << "no kb detected\n";
    else
        kb_or_null.value().print_product();

    // WINDOWS NO HOTPLUG SUPPORT YEP
    // https://github.com/libusb/libusb/issues/86
    auto libusb_callback = [](struct libusb_context *ctx, struct libusb_device *dev,
                              libusb_hotplug_event event, void *user_data) -> int {
        auto kb_or_null = static_cast<std::optional<HID::Device> *>(user_data);
        if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
            if (kb_or_null->has_value())
                return 0;

            auto new_handle_or_null =
                HID::Device::construct_handle(VENDOR_ID, PRODUCT_ID, USAGE, USAGE_PAGE);
            if (new_handle_or_null != nullptr) {
                wchar_t product[32];
                hid_get_product_string(new_handle_or_null, product, 32);
                std::wcout << "connected to " << product << "\n";
                kb_or_null->emplace(new_handle_or_null);
                send_volume(get_volume(), *kb_or_null);
            } else {
                std::cout << "received arrival event but no connection made\n";
            }
        } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
            std::cout << "disconnected from device\n";
            kb_or_null->reset();
        } else {
            std::cout << "unhandled event" << event << "\n";
        }

        return 0;
    };

    // expect this to fail on windows, ignore it and print the capability
    // maybe one day this capability changes : )
    (void)libusb_hotplug_register_callback(
        nullptr, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
        LIBUSB_HOTPLUG_ENUMERATE, VENDOR_ID, PRODUCT_ID, LIBUSB_HOTPLUG_MATCH_ANY, libusb_callback,
        &kb_or_null, nullptr);

    std::cerr << "hotplug support = "
              << (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) ? "true" : "false") << "\n";

#if defined(__MINGW32__)
    CAudioEndpointVolumeCallback callback = [&](float volume) {
        send_volume(static_cast<uint8_t>(volume * 100), kb_or_null);
    };

    SetupVolumeCallback(&callback);
    while (true) {
        Sleep(1000);
    }
#endif

    // should  have mutex locks :)
#if defined(__linux__)
    snd_ctl_t *ctl = nullptr;
    snd_ctl_open(&ctl, card, SND_CTL_READONLY);
    snd_ctl_nonblock(ctl, 0);
    snd_ctl_subscribe_events(ctl, 1);

    std::thread libusb_thread([] {
        while (true) {
            libusb_handle_events_completed(nullptr, nullptr);
        }
    });

    std::thread volume_thread([&] {
        while (true) {
            snd_ctl_event_t *event = nullptr;
            snd_ctl_event_alloca(&event);
            snd_ctl_read(ctl, event);

            uint8_t volume = get_volume();
            send_volume(volume, kb_or_null);
        }
    });

    libusb_thread.join();
    volume_thread.join();
#endif

    return 0;
}
