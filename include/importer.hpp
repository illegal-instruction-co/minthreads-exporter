#pragma once 

#include "Toolkit/StringToolkit.hpp"
#include "Toolkit/CustomAPI.hpp"

#include "Error.hpp"

#include <stdexcept>
#include <functional>
#include <string_view>
#include <map>

namespace ii {
	class Importer final {

	public:
		Importer(const std::string_view& mod) : m_moduleName(mod), m_module(CustomAPI::ModuleA(mod.data())) {

			if (!m_module)
				throw std::runtime_error(err::runtime1 + m_moduleName.data());

			m_dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(m_module);

			if (m_dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
				throw std::runtime_error(err::runtime2 + m_moduleName.data());

			m_ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<LPBYTE>(m_module) + m_dosHeader->e_lfanew);

			if (m_ntHeader->Signature != IMAGE_NT_SIGNATURE)
				throw std::runtime_error(err::runtime3 + m_moduleName.data());

			if (m_ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == 0)
				throw std::runtime_error(err::runtime4 + m_moduleName.data());

			m_exportDirectory = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(reinterpret_cast<LPBYTE>(m_module) + m_ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

			m_fetchedFunctions = Fetch();

			if (!m_fetchedFunctions.size())
				throw std::runtime_error(err::runtime5 + m_moduleName.data());
		}

		template<typename T>
		__forceinline auto Invoke(const char* fn) {

			auto it = m_fetchedFunctions.find(fn);
			if (it == m_fetchedFunctions.end())
				throw std::runtime_error(fn + err::runtime6 + m_moduleName.data());

			return [&](...) {
				using fn_t = T(WINAPI*)(...);
				return (fn_t)it->second;
			}();
		}

	private:

		std::unordered_map<std::string_view, uint64_t> Fetch() {

			std::unordered_map<std::string_view, uint64_t> res;
			PDWORD addr, name;
			PWORD ordinal;

			addr = reinterpret_cast<PDWORD>(reinterpret_cast<LPBYTE>(m_module) + m_exportDirectory->AddressOfFunctions);
			name = reinterpret_cast<PDWORD>(reinterpret_cast<LPBYTE>(m_module) + m_exportDirectory->AddressOfNames);
			ordinal = reinterpret_cast<PWORD>(reinterpret_cast<LPBYTE>(m_module) + m_exportDirectory->AddressOfNameOrdinals);

			for (int i = 0; i < m_exportDirectory->AddressOfFunctions; i++) {
				if (!StringToolkit::IsReadable(static_cast<char*>(m_module) + name[i]).GetAs<bool>())
					return res;

				if (StringToolkit::IsAlphaNumeric(static_cast<char*> (m_module) + name[i]).GetAs<bool>())
					res.insert({ std::move(std::string_view(static_cast<char*>(m_module) + name[i])), (uint64_t)m_module + addr[ordinal[i]] });
			}

			return res;
		}

	private:

		PIMAGE_DOS_HEADER m_dosHeader;

		PIMAGE_NT_HEADERS m_ntHeader;

		PIMAGE_EXPORT_DIRECTORY m_exportDirectory;

		HANDLE m_module = 0;

		std::unordered_map<std::string_view, uint64_t> m_fetchedFunctions;

		const std::string_view m_moduleName;
	};
}
