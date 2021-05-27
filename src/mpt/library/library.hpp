/* SPDX-License-Identifier: BSL-1.0 OR BSD-3-Clause */

#ifndef MPT_LIBRARY_LIBRARY_HPP
#define MPT_LIBRARY_LIBRARY_HPP



#include "mpt/base/detect.hpp"
#include "mpt/base/macros.hpp"
#include "mpt/base/namespace.hpp"
#include "mpt/base/saturate_cast.hpp"
#include "mpt/detect/dl.hpp"
#include "mpt/detect/ltdl.hpp"
#include "mpt/string/types.hpp"
#include "mpt/string_convert/convert.hpp"
#include "mpt/string_convert/convert_path.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#if MPT_OS_WINDOWS
#include <windows.h>
#elif MPT_OS_ANDROID
#elif defined(MPT_WITH_DL)
#include <dlfcn.h>
#elif defined(MPT_WITH_LTDL)
#include <ltdl.h>
#endif



namespace mpt {
inline namespace MPT_INLINE_NS {



class library {

public:
	typedef void * (*func_ptr)(); // pointer to function returning void *
	static_assert(sizeof(func_ptr) == sizeof(void *));

	enum class path_search
	{
		invalid,
		unsafe,
		default_,
		system,
		application,
		none,
	};

	enum class path_prefix
	{
		none,
		default_,
	};

	enum class path_suffix
	{
		none,
		default_,
	};

	struct path {

	public:
		path_search search = path_search::invalid;
		path_prefix prefix = path_prefix::default_;
		std::filesystem::path filename{};
		path_suffix suffix = path_suffix::default_;

		static std::filesystem::path default_prefix() {
#if MPT_OS_WINDOWS
			if constexpr (std::is_same<std::filesystem::path::value_type, wchar_t>::value) {
				return std::filesystem::path{L""};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char>::value) {
				return std::filesystem::path{""};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char32_t>::value) {
				return std::filesystem::path{U""};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char16_t>::value) {
				return std::filesystem::path{u""};
#if MPT_CXX_AT_LEAST(20)
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char8_t>::value) {
				return std::filesystem::path{u8""};
#endif
			} else {
				return std::filesystem::path{""};
			}
#else
			if constexpr (std::is_same<std::filesystem::path::value_type, wchar_t>::value) {
				return std::filesystem::path{L"lib"};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char>::value) {
				return std::filesystem::path{"lib"};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char32_t>::value) {
				return std::filesystem::path{U"lib"};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char16_t>::value) {
				return std::filesystem::path{u"lib"};
#if MPT_CXX_AT_LEAST(20)
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char8_t>::value) {
				return std::filesystem::path{u8"lib"};
#endif
			} else {
				return std::filesystem::path{"lib"};
			}
#endif
		}

		static std::filesystem::path default_suffix() {
#if MPT_OS_WINDOWS
			if constexpr (std::is_same<std::filesystem::path::value_type, wchar_t>::value) {
				return std::filesystem::path{L".dll"};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char>::value) {
				return std::filesystem::path{".dll"};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char32_t>::value) {
				return std::filesystem::path{U".dll"};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char16_t>::value) {
				return std::filesystem::path{u".dll"};
#if MPT_CXX_AT_LEAST(20)
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char8_t>::value) {
				return std::filesystem::path{u8".dll"};
#endif
			} else {
				return std::filesystem::path{".dll"};
			}
#elif MPT_OS_ANDROID || defined(MPT_WITH_DL)
			if constexpr (std::is_same<std::filesystem::path::value_type, wchar_t>::value) {
				return std::filesystem::path{L".so"};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char>::value) {
				return std::filesystem::path{".so"};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char32_t>::value) {
				return std::filesystem::path{U".so"};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char16_t>::value) {
				return std::filesystem::path{u".so"};
#if MPT_CXX_AT_LEAST(20)
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char8_t>::value) {
				return std::filesystem::path{u8".so"};
#endif
			} else {
				return std::filesystem::path{".so"};
			}
#else
			if constexpr (std::is_same<std::filesystem::path::value_type, wchar_t>::value) {
				return std::filesystem::path{L""};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char>::value) {
				return std::filesystem::path{""};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char32_t>::value) {
				return std::filesystem::path{U""};
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char16_t>::value) {
				return std::filesystem::path{u""};
#if MPT_CXX_AT_LEAST(20)
			} else if constexpr (std::is_same<std::filesystem::path::value_type, char8_t>::value) {
				return std::filesystem::path{u8""};
#endif
			} else {
				return std::filesystem::path{""};
			}
#endif
		}

		std::optional<std::filesystem::path> get_effective_filename() const {
			switch (search) {
				case library::path_search::unsafe:
					break;
				case library::path_search::default_:
					break;
				case library::path_search::system:
					if (filename.is_absolute()) {
						return std::nullopt;
					}
					break;
				case library::path_search::application:
					if (filename.is_absolute()) {
						return std::nullopt;
					}
					break;
				case library::path_search::none:
					if (filename.is_relative()) {
						return std::nullopt;
					}
					break;
				case library::path_search::invalid:
					return std::nullopt;
					break;
			}
			std::filesystem::path result{};
			result += filename.parent_path();
			result += ((prefix == path_prefix::default_) ? default_prefix() : std::filesystem::path{});
			result += filename.filename();
			result += ((suffix == path_suffix::default_) ? default_suffix() : std::filesystem::path{});
			return result;
		}
	};

#if MPT_OS_WINDOWS

private:
	HMODULE m_hModule = NULL;

	library(HMODULE hModule)
		: m_hModule(hModule) {
		return;
	}

public:
	library(const library &) = delete;
	library & operator=(const library &) = delete;

	library(library && other) noexcept
		: m_hModule(other.m_hModule) {
		other.m_hModule = NULL;
	}

	library & operator=(library && other) noexcept {
		FreeLibrary(m_hModule);
		m_hModule = other.m_hModule;
		other.m_hModule = NULL;
		return *this;
	}

private:
#if !MPT_OS_WINDOWS_WINRT
#if (_WIN32_WINNT < 0x0602)

	static std::filesystem::path get_application_path() {
		std::vector<TCHAR> path(MAX_PATH);
		while (GetModuleFileName(0, path.data(), mpt::saturate_cast<DWORD>(path.size())) >= path.size()) {
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				return std::filesystem::path{};
			}
			path.resize(mpt::exponential_grow(path.size()));
		}
		return std::filesystem::absolute(mpt::convert<std::filesystem::path>(mpt::winstring(path.data())));
	}

	static std::filesystem::path get_system_path() {
		DWORD size = GetSystemDirectory(nullptr, 0);
		std::vector<TCHAR> path(size + 1);
		if (!GetSystemDirectory(path.data(), size + 1)) {
			return std::filesystem::path{};
		}
		return mpt::convert<std::filesystem::path>(mpt::winstring(path.data()));
	}

#endif
#endif // !MPT_OS_WINDOWS_WINRT

public:
	static std::optional<library> load(mpt::library::path path) {

		HMODULE hModule = NULL;

		std::optional<std::filesystem::path> optionalfilename = path.get_effective_filename();
		if (!optionalfilename) {
			return std::nullopt;
		}
		std::filesystem::path & filename = optionalfilename.value();
		if (filename.empty()) {
			return std::nullopt;
		}

#if MPT_OS_WINDOWS_WINRT

#if (_WIN32_WINNT < 0x0602)
		MPT_UNUSED(path);
		return std::nullopt;
#else  // Windows 8
		switch (path.search) {
			case library::path_search::unsafe:
				hModule = LoadPackagedLibrary(mpt::convert<mpt::winstring>(filename).c_str(), 0);
				break;
			case library::path_search::default_:
				hModule = LoadPackagedLibrary(mpt::convert<mpt::winstring>(filename).c_str(), 0);
				break;
			case library::path_search::system:
				hModule = NULL; // Only application packaged libraries can be loaded dynamically in WinRT
				break;
			case library::path_search::application:
				hModule = LoadPackagedLibrary(mpt::convert<mpt::winstring>(filename).c_str(), 0);
				break;
			case library::path_search::none:
				hModule = NULL; // Absolute path is not supported in WinRT
				break;
			case library::path_search::invalid:
				hModule = NULL;
				break;
		}
#endif // Windows Version

#else // !MPT_OS_WINDOWS_WINRT

#if (_WIN32_WINNT >= 0x0602) // Windows 8

		switch (path.search) {
			case library::path_search::unsafe:
				hModule = LoadLibrary(mpt::convert<mpt::winstring>(filename).c_str());
				break;
			case library::path_search::default_:
				hModule = LoadLibraryEx(mpt::convert<mpt::winstring>(filename).c_str(), NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
				break;
			case library::path_search::system:
				hModule = LoadLibraryEx(mpt::convert<mpt::winstring>(filename).c_str(), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
				break;
			case library::path_search::application:
				hModule = LoadLibraryEx(mpt::convert<mpt::winstring>(filename).c_str(), NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
				break;
			case library::path_search::none:
				hModule = LoadLibraryEx(mpt::convert<mpt::winstring>(filename).c_str(), NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
				break;
			case library::path_search::invalid:
				hModule = NULL;
				break;
		}

#else // < Windows 8

		switch (path.search) {
			case library::path_search::unsafe:
				hModule = LoadLibrary(mpt::convert<mpt::winstring>(filename).c_str());
				break;
			case library::path_search::default_:
				hModule = LoadLibrary(mpt::convert<mpt::winstring>(filename).c_str());
				break;
			case library::path_search::system:
				{
					std::filesystem::path system_path = get_system_path();
					if (system_path.empty()) {
						hModule = NULL;
					} else {
						hModule = LoadLibrary(mpt::convert<mpt::winstring>(system_path / filename).c_str());
					}
				}
				break;
			case library::path_search::application:
				{
					std::filesystem::path application_path = get_application_path();
					if (application_path.empty()) {
						hModule = NULL;
					} else {
						hModule = LoadLibrary(mpt::convert<mpt::winstring>(application_path / filename).c_str());
					}
				}
				break;
			case library::path_search::none:
				hModule = LoadLibrary(mpt::convert<mpt::winstring>(filename).c_str());
				break;
			case library::path_search::invalid:
				hModule = NULL;
				break;
		}

#endif // MPT_OS_WINDOWS_WINRT

#endif

		return library{hModule};
	}

	func_ptr get_address(const std::string & symbol) const {
		return reinterpret_cast<func_ptr>(::GetProcAddress(m_hModule, symbol.c_str()));
	}

	~library() {
		FreeLibrary(m_hModule);
	}

#elif defined(MPT_WITH_DL)

private:
	void * handle = nullptr;

	library(void * handle_)
		: handle(handle_) {
		return;
	}

public:
	library(const library &) = delete;
	library & operator=(const library &) = delete;

	library(library && other) noexcept
		: handle(other.handle) {
		other.handle = NULL;
	}

	library & operator=(library && other) noexcept {
		dlclose(handle);
		m_hModule = other.handle;
		other.handle = NULL;
		return *this;
	}

public:
	static std::optional<library> load(mpt::library::path path) {
		std::optional<std::filesystem::path> optionalfilename = path.get_effective_filename();
		if (!optionalfilename) {
			return std::nullopt;
		}
		std::filesystem::path & filename = optionalfilename.value();
		if (filename.empty()) {
			return std::nullopt;
		}
		void * handle = dlopen(filename.native().c_str(), RTLD_NOW);
		if (!handle) {
			return std::nullopt;
		}
		return library{handle};
	}

	func_ptr get_address(const std::string & symbol) const {
		return reinterpret_cast<func_ptr>(dlsym(handle, symbol.c_str()));
	}

	~library() {
		dlclose(handle);
	}

#elif defined(MPT_WITH_LTDL)

private:
	lt_dlhandle handle{};

	library(lt_dlhandle handle_)
		: handle(handle_) {
		return;
	}

public:
	library(const library &) = delete;
	library & operator=(const library &) = delete;

	library(library && other) noexcept
		: handle(other.handle) {
		other.handle = NULL;
	}

	library & operator=(library && other) noexcept {
		dlclose(handle);
		m_hModule = other.handle;
		other.handle = NULL;
		return *this;
	}

public:
	static std::optional<library> load(mpt::library::path path) {
		if (lt_dlinit() != 0) {
			return std::nullopt;
		}
		std::optional<std::filesystem::path> optionalfilename = path.get_effective_filename();
		if (!optionalfilename) {
			return std::nullopt;
		}
		std::filesystem::path & filename = optionalfilename.value();
		if (filename.empty()) {
			return std::nullopt;
		}
		lt_dlhandle handle = lt_dlopenext(filename.native().c_str());
		if (!handle) {
			return std::nullopt;
		}
		return library{handle};
	}

	func_ptr get_address(const std::string & symbol) const {
		return reinterpret_cast<func_ptr>(dlsym(handle, symbol.c_str()));
	}

	~library() {
		lt_dlclose(handle);
		lt_dlexit();
	}

#else

private:
	void * handle = nullptr;

	library(void * handle_)
		: handle(handle_) {
		return;
	}

public:
	library(const library &) = delete;
	library & operator=(const library &) = delete;

	library(library && other) noexcept
		: handle(other.handle) {
		other.handle = nullptr;
	}

	library & operator=(library && other) noexcept {
		handle = other.handle;
		other.handle = nullptr;
		return *this;
	}

public:
	static std::optional<library> load(mpt::library::path path) {
		MPT_UNUSED(path);
		return std::nullopt;
	}

	func_ptr get_address(const std::string & symbol) const {
		MPT_UNUSED(symbol);
		return nullptr;
	}

	~library() {
		return;
	}

#endif

	template <typename Tfunc>
	bool bind(Tfunc *& f, const std::string & symbol) const {
#if !(MPT_OS_WINDOWS && MPT_COMPILER_GCC)
		// MinGW64 std::is_function is always false for non __cdecl functions.
		// Issue is similar to <https://connect.microsoft.com/VisualStudio/feedback/details/774720/stl-is-function-bug>.
		static_assert(std::is_function<Tfunc>::value);
#endif
		const func_ptr addr = get_address(symbol);
		f = reinterpret_cast<Tfunc *>(addr);
		return (addr != nullptr);
	}
};



} // namespace MPT_INLINE_NS
} // namespace mpt



#endif // MPT_LIBRARY_LIBRARY_HPP
