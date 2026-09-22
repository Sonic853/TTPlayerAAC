#include "common.h"
#include <cmath>
#include <shlwapi.h>

namespace ttp::aac {
namespace {
constexpr HRESULT nero_error(unsigned n) {
    return HRESULT(0x8bda0600U + n);
}
constexpr GUID n_enumerate{0xfc2e77b1, 0x6278, 0x452b, {0x97, 0x56, 0x7a, 0xc2, 0x4e, 0x80, 0x77, 0x78}};
constexpr GUID n_factory{0xbb980e93, 0x637f, 0x4373, {0x8f, 0x7d, 0xa8, 0xaa, 0x10, 0x3c, 0xa7, 0x32}};
constexpr GUID n_config{0xf50907a0, 0xc032, 0x4935, {0xa3, 0xe0, 0x83, 0x20, 0x03, 0xe4, 0x4f, 0x06}};
constexpr GUID n_path{0xfa6a3cf1, 0xe837, 0x42c2, {0xad, 0xdc, 0x8b, 0x50, 0x44, 0xd6, 0x84, 0x8e}};
constexpr GUID n_write{0xb011fb4d, 0x3a88, 0x4a32, {0xbc, 0xd6, 0xdb, 0xf0, 0xdd, 0xbf, 0xcf, 0xc0}};
constexpr GUID n_control{0xb388f5e3, 0x94b2, 0x40ff, {0xbc, 0xe9, 0x0a, 0xc8, 0xdb, 0xf5, 0x51, 0xcb}};
constexpr GUID n_tags{0xfb863baf, 0xab38, 0x4851, {0x8b, 0x61, 0x14, 0x02, 0xb2, 0x7d, 0x2f, 0xa4}};
constexpr GUID n_track{0xb6062833, 0xece6, 0x4716, {0xa6, 0xec, 0xf9, 0x6a, 0x08, 0x5a, 0xac, 0x14}};
// Nero mixes stdcall IUnknown with thiscall extension methods. The factory's
// IUnknown subobject is at +4 (600037D1 and 600038D0), not its primary vptr.
template <class R, class... A> R call(void *object, size_t slot, A... args) {
    require(object != nullptr, nero_error(10));
    auto table = *static_cast<void ***>(object);
    return reinterpret_cast<R(__thiscall *)(void *, A...)>(table[slot])(object, args...);
}
struct Libraries {
    std::vector<HMODULE> modules;
    ~Libraries() {
        for (auto it = modules.rbegin(); it != modules.rend(); ++it)
            FreeLibrary(*it);
    }
    HMODULE load(const std::wstring &file) {
        auto m = LoadLibraryExW(file.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        require(m != nullptr, nero_error(7));
        modules.push_back(m);
        return m;
    }
};
struct FactoryRef {
    void *value{};
    IUnknown *unknown() const {
        return value ? reinterpret_cast<IUnknown *>(static_cast<BYTE *>(value) + 4) : nullptr;
    }
    ~FactoryRef() {
        if (auto *p = unknown())
            p->Release();
    }
};
struct Nero {
    Libraries libraries;
    ComPtr<IUnknown> primary;
    FactoryRef factory;
    void open() {
        std::wstring dir(32768, L'\0');
        DWORD n = GetModuleFileNameW(module, dir.data(), DWORD(dir.size()));
        require(n && n < dir.size(), nero_error(7));
        dir.resize(n);
        auto slash = dir.find_last_of(L"\\/");
        require(slash != std::wstring::npos);
        dir.resize(slash + 1);
        libraries.load(dir + L"aacenc32.dll");
        libraries.load(dir + L"NeroIPP.dll");
        auto aac = libraries.load(dir + L"Aac.dll");
        auto get = reinterpret_cast<bool(__cdecl *)(IUnknown **)>(
            GetProcAddress(aac, "NERO_PLUGIN_GetPrimaryAudioObject"));
        require(get != nullptr, nero_error(7));
        require(get(primary.put()) && primary, nero_error(7));
        ComPtr<IUnknown> list;
        check(primary->QueryInterface(n_enumerate, reinterpret_cast<void **>(list.put())));
        int count = call<int>(list.p, 3);
        require(count >= 0 && count < 10000, nero_error(7));
        for (int i = 0; i < count; ++i) {
            ComPtr<IUnknown> entry;
            if (!call<bool>(list.p, 4, i, entry.put()) || !entry)
                continue;
            if (call<int>(entry.p, 4) == 4) {
                check(entry->QueryInterface(n_factory, &factory.value));
                break;
            }
        }
        require(factory.value != nullptr, nero_error(7));
    }
};
std::string ansi_path(const wchar_t *value) {
    std::wstring absolute(32768, L'\0');
    DWORD length = GetFullPathNameW(value, DWORD(absolute.size()), absolute.data(), nullptr);
    require(length && length < absolute.size(), nero_error(6));
    absolute.resize(length);
    value = absolute.c_str();
    BOOL lossy{};
    int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value, -1, nullptr, 0, nullptr, &lossy);
    require(n > 0);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value, -1, out.data(), n, nullptr, &lossy);
    if (lossy) {
        std::wstring path(value);
        auto slash = path.find_last_of(L"\\/");
        require(slash != std::wstring::npos, nero_error(6));
        auto parent = path.substr(0, slash);
        std::wstring shortname(32768, L'\0');
        DWORD k = GetShortPathNameW(parent.c_str(), shortname.data(), DWORD(shortname.size()));
        require(k && k < shortname.size(), nero_error(6));
        shortname.resize(k);
        shortname += path.substr(slash);
        lossy = FALSE;
        n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, shortname.c_str(), -1, nullptr, 0, nullptr,
                                &lossy);
        out.assign(n, '\0');
        WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, shortname.c_str(), -1, out.data(), n, nullptr,
                            &lossy);
        require(!lossy, nero_error(6));
    }
    return out;
}
class NeroEncoder final : public Encoder, public Metadata {
    LONG references_{1};
    std::unique_ptr<Nero> nero_;
    ComPtr<IUnknown> writer_, control_, tags_, track_;
    WORD bits_{}, channels_{};
    bool floating_{}, started_{};
    std::vector<short> pcm_;

  public:
    ~NeroEncoder() {
        if (started_ && control_)
            call<bool>(control_.p, 4, DWORD(0));
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (same(iid, IID_IUnknown) || same(iid, iid_encoder))
            *out = static_cast<Encoder *>(this);
        else if (same(iid, iid_metadata))
            *out = static_cast<Metadata *>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&references_); }
    ULONG STDMETHODCALLTYPE Release() override {
        auto n = InterlockedDecrement(&references_);
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE Open(const wchar_t *path, WAVEFORMATEX *f) override {
        if (!path || !f)
            return E_POINTER;
        return protect([&]() -> HRESULT {
            require(!nero_, E_UNEXPECTED);
            DWORD format = f->wFormatTag;
            if (format == WAVE_FORMAT_EXTENSIBLE && f->cbSize >= 22) {
                GUID sub{};
                std::memcpy(&sub, reinterpret_cast<BYTE *>(f) + 24, 16);
                GUID expected = aac_subtype;
                expected.Data1 = sub.Data1;
                require(same(sub, expected), nero_error(2));
                format = sub.Data1;
            }
            require(format == WAVE_FORMAT_PCM || format == WAVE_FORMAT_IEEE_FLOAT, nero_error(2));
            floating_ = format == WAVE_FORMAT_IEEE_FLOAT;
            require(!floating_ || f->wBitsPerSample == 64, nero_error(3));
            bits_ = f->wBitsPerSample;
            channels_ = f->nChannels;
            require(channels_ && channels_ <= 64 && f->nSamplesPerSec && f->nSamplesPerSec <= 48000,
                    nero_error(2));
            require(floating_ || bits_ == 8 || bits_ == 16 || bits_ == 24 || bits_ == 32, nero_error(3));
            auto nero = std::make_unique<Nero>();
            nero->open();
            DWORD pcm_format[] = {f->nSamplesPerSec, 16, channels_};
            ComPtr<IUnknown> object;
            require(call<bool>(nero->factory.value, 2, object.put(), pcm_format, DWORD(0)) && object,
                    nero_error(8));
            ComPtr<IUnknown> path_writer;
            check(object->QueryInterface(n_path, reinterpret_cast<void **>(path_writer.put())));
            auto name = ansi_path(path);
            require(call<bool>(path_writer.p, 3, name.c_str(), DWORD(0)), nero_error(6));
            ComPtr<IUnknown> writer, control, tags, track;
            check(object->QueryInterface(n_write, reinterpret_cast<void **>(writer.put())));
            check(object->QueryInterface(n_control, reinterpret_cast<void **>(control.put())));
            object->QueryInterface(n_tags, reinterpret_cast<void **>(tags.put()));
            object->QueryInterface(n_track, reinterpret_cast<void **>(track.put()));
            nero_ = std::move(nero);
            writer_ = std::move(writer);
            control_ = std::move(control);
            tags_ = std::move(tags);
            track_ = std::move(track);
            return S_OK;
        });
    }
    HRESULT STDMETHODCALLTYPE Start() override {
        return protect([&]() -> HRESULT {
            require(control_ && !started_, E_UNEXPECTED);
            require(call<bool>(control_.p, 3, DWORD(0)), nero_error(8));
            started_ = true;
            return S_OK;
        });
    }
    HRESULT STDMETHODCALLTYPE Finish() override {
        return protect([&]() -> HRESULT {
            require(control_ && started_, E_UNEXPECTED);
            started_ = false;
            return call<bool>(control_.p, 4, DWORD(0)) ? S_OK : nero_error(12);
        });
    }
    HRESULT STDMETHODCALLTYPE Write(Buffer *buffer, DWORD *written) override {
        if (written)
            *written = 0;
        if (!buffer)
            return E_POINTER;
        return protect([&]() -> HRESULT {
            require(started_ && writer_, E_UNEXPECTED);
            BYTE *p{};
            DWORD n{};
            check(buffer->Data(&p, &n));
            if (!n)
                return S_OK;
            require(p && bits_ && n % (channels_ * (bits_ / 8)) == 0, E_INVALIDARG);
            size_t count = n / (bits_ / 8);
            pcm_.resize(count);
            for (size_t i = 0; i < count; ++i) {
                int value{};
                if (floating_) {
                    double v{};
                    std::memcpy(&v, p + i * 8, 8);
                    if (!std::isfinite(v))
                        v = 0;
                    v = std::clamp(v, -1.0, 1.0);
                    value = int(std::nearbyint(v * 32768.0));
                } else if (bits_ == 8)
                    value = (int(p[i]) - 128) * 256;
                else {
                    auto *src = p + i * (bits_ / 8) + (bits_ / 8 - 2);
                    value = short(WORD(src[0]) | (WORD(src[1]) << 8));
                }
                pcm_[i] = short(std::clamp(value, -32768, 32767));
            }
            DWORD used{};
            require(call<bool>(writer_.p, 3, pcm_.data(), DWORD(pcm_.size() * 2), &used, DWORD(0)),
                    E_FAIL);
            if (written)
                *written = n;
            return S_OK;
        });
    }
    HRESULT STDMETHODCALLTYPE Extension(wchar_t **out) override {
        if (out)
            *out = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Count(DWORD *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE At(DWORD, wchar_t **, wchar_t **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Get(const char *, wchar_t **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Set(const char *name, const wchar_t *value) override {
        if (!name)
            return E_POINTER;
        if (!tags_)
            return E_NOINTERFACE;
        return protect([&]() -> HRESULT {
            auto v = utf8(value ? value : L"");
            const char *names[] = {"Title", "Artist", "Album", "Date", "Genre"};
            for (size_t i = 0; i < 5; ++i)
                if (!_stricmp(name, names[i])) {
                    call<void>(tags_.p, i + 3, v.c_str());
                    return S_OK;
                }
            if (!_stricmp(name, "Tracknumber") && track_) {
                call<void>(track_.p, 4, int(wcstol(value ? value : L"0", nullptr, 10)) - 1);
                return S_OK;
            }
            return E_INVALIDARG;
        });
    }
};
} // namespace
HRESULT configure_nero(HWND parent) {
    if (!parent)
        return E_POINTER;
    return protect([&]() -> HRESULT {
        Nero nero;
        nero.open();
        ComPtr<IUnknown> config;
        check(nero.factory.unknown()->QueryInterface(n_config, reinterpret_cast<void **>(config.put())));
        return call<bool>(config.p, 3) ? S_OK : nero_error(5);
    });
}
HRESULT nero_available(wchar_t **reason) {
    if (reason)
        *reason = nullptr;
    HRESULT hr = protect([&]() -> HRESULT {
        Nero nero;
        nero.open();
        return S_OK;
    });
    if (FAILED(hr) && reason)
        text(L"Nero AAC encoder components are unavailable.", reason);
    return hr;
}
HRESULT make_encoder(void **out) {
    if (!out)
        return E_POINTER;
    *out = nullptr;
    return protect([&]() -> HRESULT {
        *out = static_cast<Encoder *>(new NeroEncoder);
        return S_OK;
    });
}
} // namespace ttp::aac
