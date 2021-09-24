// clang-format off
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
// clang-format on

#include <hidapi.h>
#include <libusb.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>

// TODO: add more cool stuff

template <typename T> class ScopeGuard {
  private:
    T m_callback;

  public:
    ScopeGuard(T callback) : m_callback(std::move(callback)) {
    }

    ~ScopeGuard() {
        m_callback();
    }
};

#if defined(__linux__)
#include <alsa/asoundlib.h>

#include <cmath>
#include <signal.h>
#include <thread>

static const char *mix_name = "Master";
static const char *card = "default";
static int mix_index = 0;

void alsa_connect(snd_mixer_t **handle, snd_mixer_elem_t **elem) {
    snd_mixer_selem_id_t *sid = nullptr;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, mix_index);
    snd_mixer_selem_id_set_name(sid, mix_name);

    // HOPE NOTHING FAILS HEY
    snd_mixer_open(handle, 0);

    if ((snd_mixer_attach(*handle, card)) < 0) {
        snd_mixer_close(*handle);
    }
    if ((snd_mixer_selem_register(*handle, NULL, NULL)) < 0) {
        snd_mixer_close(*handle);
    }
    if (snd_mixer_load(*handle) < 0) {
        snd_mixer_close(*handle);
    }

    *elem = snd_mixer_find_selem(*handle, sid);
    if (!elem) {
        snd_mixer_close(*handle);
    }
}

bool get_mute() {
    snd_mixer_t *handle = nullptr;
    snd_mixer_elem_t *elem = nullptr;
    alsa_connect(&handle, &elem);

    ScopeGuard guard = [&] { snd_mixer_close(handle); };

    int mute;
    snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_UNKNOWN, &mute);

    return mute ? false : true;
}

uint8_t get_volume() {
    snd_mixer_t *handle = nullptr;
    snd_mixer_elem_t *elem = nullptr;
    alsa_connect(&handle, &elem);

    ScopeGuard guard = [&] { snd_mixer_close(handle); };

    // https://github.com/alsa-project/alsa-utils/blob/7a7e064f83f128e4e115c24ef15ba6788b1709a6/alsamixer/volume_mapping.c
    // thanks :)
    long minv, maxv, outvol;
    snd_mixer_selem_get_playback_dB_range(elem, &minv, &maxv);
    // can fail
    snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_UNKNOWN, &outvol);

    double normalised = pow(10, (outvol - maxv) / 6000.0);
    double min_norm = pow(10, (minv - maxv) / 6000.0);
    normalised = (normalised - min_norm) / (1 - min_norm);

    return static_cast<uint8_t>(round(normalised * 100));
}
#endif

#if defined(__MINGW32__)
#include <Windows.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

#include <utility>

template <typename T, typename T1>
class CAudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback {
    LONG _cRef;
    T m_volume_callback;
    T1 m_mute_callback;
    bool mute = false;
    float volume = 0;

  public:
    CAudioEndpointVolumeCallback(T volume_callback, T1 mute_callback)
        : _cRef(1), m_volume_callback(std::move(volume_callback)),
          m_mute_callback(std::move(mute_callback)) {
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
        bool old_mute = mute;
        float old_volume = volume;

        mute = pNotify->bMuted;
        volume = pNotify->fMasterVolume;

        if (mute != old_mute) {
            m_mute_callback(pNotify->bMuted);
        }

        if (!mute && volume != old_volume) {
            m_volume_callback(pNotify->fMasterVolume);
        }

        return S_OK;
    }
};

template <typename T>
void SetupVolumeCallback(IAudioEndpointVolumeCallback *callback, T initialState) {
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

    WINBOOL mute;
    pAudioEndpointVolume->GetMute(&mute);
    FLOAT volume;
    pAudioEndpointVolume->GetMasterVolumeLevelScalar(&volume);

    initialState(volume, mute);

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

#include "../qmk/common/hid_commands.h"
#define UNUSED(x) static_cast<void>(x)
// must match those defined in config.h
#define VENDOR_ID  0x320F
#define PRODUCT_ID 0x5044
// defaults -> https://beta.docs.qmk.fm/using-qmk/software-features/feature_rawhid
#define USAGE      0x61
#define USAGE_PAGE 0xFF60
class HID {
  public:
    static HID hid;
    HID() {
        if (hid_init()) {
            abort();
        }
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

            if (!path) {
                return nullptr;
            }

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

        template <typename OStream> friend OStream &operator<<(OStream &os, const Device &dev) {
            wchar_t product[32];
            char utf8_prod[32];
            hid_get_product_string(dev.m_handle, product, 32);
            std::wcstombs(utf8_prod, product, 32);

            return os << utf8_prod;
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

void send_buffer(std::optional<HID::Device> &kb_or_null, uint8_t *buffer) {
    if (kb_or_null.has_value()) {
        kb_or_null->write(buffer, 32);
        char s[32];
        std::wcstombs(s, kb_or_null->error(), 32);
        spdlog::info("writing: {} {} to {} -> {}", buffer[2], buffer[3], *kb_or_null, s);
    }
}

// buf[0] is the report ID, should be set to 0
void send_mute(std::optional<HID::Device> &kb_or_null, bool mute) {
    uint8_t buf[32] = {0};
    buf[1] = HIDCommands::VIA_LIGHTING_SET_VALUE;
    buf[2] = HIDCommands::MUTE_COMMAND;
    buf[3] = static_cast<uint8_t>(mute);
    send_buffer(kb_or_null, buf);
}

void send_volume(std::optional<HID::Device> &kb_or_null, uint8_t volume) {
    uint8_t buf[32] = {0};
    buf[1] = HIDCommands::VIA_LIGHTING_SET_VALUE;
    buf[2] = HIDCommands::VOLUME_COMMAND;
    buf[3] = volume;
    send_buffer(kb_or_null, buf);
}

// protect handle access across 2 threads
std::mutex g_protect_handle;

int main(void) {
    spdlog::set_pattern("[%^%7l%$]: %v");
    libusb_init(nullptr);
    const libusb_version *libusb_ver = libusb_get_version();
    spdlog::info("libusb: v{}.{}.{}", libusb_ver->major, libusb_ver->minor, libusb_ver->micro);
    const struct hid_api_version *hidapi_ver = hid_version();
    spdlog::info("hidapi: v{}.{}.{}", hidapi_ver->major, hidapi_ver->minor, hidapi_ver->patch);

    ScopeGuard guard = [&] { libusb_exit(nullptr); };

    // lets try to connect as soon as we start
    auto kb_or_null = HID::Device::construct(VENDOR_ID, PRODUCT_ID, USAGE, USAGE_PAGE);
    // doesn't matter if we fail, on linux we setup a hotplug handler

    if (kb_or_null.has_value()) {
        spdlog::info("device connected: {}", *kb_or_null);
    } else {
        spdlog::info("no kb detected");
    }

#if defined(__linux__)
    send_mute(kb_or_null, get_mute());
    send_volume(kb_or_null, get_volume());

    // WINDOWS NO HOTPLUG SUPPORT YEP
    // https://github.com/libusb/libusb/issues/86
    auto libusb_callback = [](struct libusb_context *ctx, struct libusb_device *dev,
                              libusb_hotplug_event event, void *user_data) -> int {
        std::lock_guard<std::mutex> guard(g_protect_handle);
        auto kb_or_null = static_cast<std::optional<HID::Device> *>(user_data);
        if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
            if (kb_or_null->has_value()) {
                return 0;
            }

            auto new_handle_or_null =
                HID::Device::construct_handle(VENDOR_ID, PRODUCT_ID, USAGE, USAGE_PAGE);
            if (new_handle_or_null != nullptr) {
                kb_or_null->emplace(new_handle_or_null);
                spdlog::info("device connected: {}", **kb_or_null);

                // provide initial state
                send_mute(*kb_or_null, get_mute());
                send_volume(*kb_or_null, get_volume());
            } else {
                spdlog::warn("received arrival event but no connection made, no perms possibly?");
            }
        } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
            spdlog::info("disconnected from device");
            kb_or_null->reset();
        } else {
            spdlog::error("unhandled event");
        }

        return 0;
    };

    // expect this to fail on windows, ignore it and print the capability
    // maybe one day this capability changes : )
    (void)libusb_hotplug_register_callback(
        nullptr, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
        LIBUSB_HOTPLUG_ENUMERATE, VENDOR_ID, PRODUCT_ID, LIBUSB_HOTPLUG_MATCH_ANY, libusb_callback,
        &kb_or_null, nullptr);

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

            {
                std::lock_guard<std::mutex> guard(g_protect_handle);
                unsigned int event_id = snd_ctl_event_elem_get_numid(event);
                if (event_id == 36) {
                    bool mute = get_mute();
                    send_mute(kb_or_null, mute);
                }

                if (event_id == 35) {
                    uint8_t volume = get_volume();
                    send_volume(kb_or_null, volume);
                }
            }
        }
    });

    libusb_thread.join();
    volume_thread.join();
#endif

    spdlog::info("setting up hotplug callback");
    spdlog::info("libusb hotplug support = {}",
                 (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) ? "true" : "false"));

#if defined(__MINGW32__)
    CAudioEndpointVolumeCallback callback = {
        [&](float volume) { send_volume(kb_or_null, static_cast<uint8_t>(volume * 100)); },
        [&](bool mute) { send_mute(kb_or_null, mute); }};

    SetupVolumeCallback(&callback, [&](float volume, WINBOOL mute) {
        // callback sends the initial date from the connection
        send_volume(kb_or_null, static_cast<uint8_t>(volume * 100));
        send_mute(kb_or_null, mute);
    });
    while (true) {
        Sleep(1000);
    }
#endif

    return 0;
}
