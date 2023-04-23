#include <fstream>
#include <iostream>
#include <sstream>
#include <windows.h>
#include "detours.h"

// Some partially reversed Noesis structs, maybe useful later?
struct Noesis_Stream {
    void* unk00; // destroy function?
    int32_t numReferences;
    uint8_t unk0C[4];
    uint8_t* data;
    uint32_t length;
    uint32_t unk1C;
};
template <typename T> struct Noesis_Ptr {
    T* pointer;
    uint32_t numReferences;
};
class Noesis_BaseComponent;
struct Noesis_Uri {
    uint8_t unk00[8];
    char str[512];
    uint8_t unk208[2];
    bool isValid : 8;
    bool isAbsolute : 8;
};

// Game specific structs
struct XamlProviderEntry {
    // Describes a single Xaml file
    char* filename;
    uint8_t* contents;
    uint32_t length;
    uint16_t unk14;
    uint16_t unk16;
};
struct XamlProvider {
    // Noesis members (?)
    void** vtable; // vtable[5] = LoadXaml
    uint8_t unk00[0x28];

    // Game specific (?) members
    XamlProviderEntry* entries;
    uint32_t numEntries;
    uint8_t unk40[0xC];
};

static void(__stdcall* NoesisInit)(void) = NULL;
static Noesis_Ptr<Noesis_BaseComponent>* (__stdcall* Noesis_GUI_LoadXaml_String)(void *_this, char const *) = NULL;
static void(__stdcall* Noesis_GUI_SetXamlProvider)(XamlProvider* xamlProvider) = NULL;

void __stdcall MyNoesisInit(void) {
    std::cout << "MyNoesisInit called" << std::endl;
    //while (true) {}
    NoesisInit();
}

Noesis_Ptr<Noesis_BaseComponent>* MyNoesis_GUI_LoadXaml_String(void *_this, char const* str) {
    std::cout << "Noesis::GUI::LoadXaml_String(" << str << ")" << std::endl;
    return Noesis_GUI_LoadXaml_String(_this, str);
}

void MyNoesis_GUI_SetXamlProvider(XamlProvider *xamlProvider) {
    std::cout << "Noesis::GUI::SetXamlProvider(" << xamlProvider << ")" << std::endl;
    //while (true) {}

    // Dump XAML files
    /*for (size_t i = 0; i < xamlProvider->numEntries; i++) {
        std::ofstream xamlfile(xamlProvider->entries[i].filename, std::ios::binary);
        xamlfile << std::string((const char*)xamlProvider->entries[i].contents, xamlProvider->entries[i].length);
    }*/

    // Inject new XAML
    for (size_t i = 0; i < xamlProvider->numEntries; i++) {
        std::stringstream filename;
        filename << "xaml\\" << xamlProvider->entries[i].filename;
        std::ifstream xamlfile(filename.str(), std::ios::binary);
        if (xamlfile.good()) {
            std::stringstream contents;
            contents << xamlfile.rdbuf();

            uint32_t len = contents.tellp();
            uint8_t *newContents = new uint8_t[len]; // should only be called once...
            memcpy(newContents, contents.str().c_str(), len);

            xamlProvider->entries[i].contents = newContents;
            xamlProvider->entries[i].length = len;

            std::cout << "Replacing " << xamlProvider->entries[i].filename << "..." << std::endl;
        }
    }

    Noesis_GUI_SetXamlProvider(xamlProvider);
}

extern "C" __declspec(dllexport) BOOL WINAPI ChaudLoaderInit(const uint8_t * userdata, size_t size) {
    if (DetourIsHelperProcess()) {
        std::cout << "DetourIsHelperProcess == false" << std::endl;
        return false;
    }

    if (auto result = DetourRestoreAfterWith()) {
        std::cout << "DetourRestoreAfterWith() returned " << result << std::endl;
        return false;
    }
    std::cout << "DetourRestoreAfterWith() ok" << std::endl;

    if (auto result = DetourTransactionBegin()) {
        std::cout << "DetourTransactionBegin() returned " << result << std::endl;
        return false;
    }
    std::cout << "DetourTransactionBegin() ok" << std::endl;

    if (auto result = DetourUpdateThread(GetCurrentThread())) {
        std::cout << "DetourUpdateThread() returned " << result << std::endl;
        return false;
    }
    std::cout << "DetourUpdateThread() ok" << std::endl;

    HMODULE noesisDLL = GetModuleHandle(L"Noesis.dll");
    std::cout << "Noesis.dll @ " << GetModuleHandle(TEXT("Noesis.dll")) << std::endl;
    if (noesisDLL == NULL) {
        std::cout << "Noesis.dll is not loaded!" << std::endl;
        return false;
    }

    {
        FARPROC fn = GetProcAddress(noesisDLL, "?Init@Noesis@@YAXXZ");
        if (fn == NULL) {
            std::cout << "Cannot find Noesis::Init()!" << std::endl;
            return false;
        }
        std::cout << "Noesis::Init() @ " << fn << std::endl;
        NoesisInit = (void(__stdcall*)(void))fn;
    }

    {
        FARPROC fn = GetProcAddress(noesisDLL, "?SetXamlProvider@GUI@Noesis@@YAXPEAVXamlProvider@2@@Z");
        if (fn == NULL) {
            std::cout << "Cannot find Noesis::GUI::SetXamlProvider(XamlProvider *)" << std::endl;
            return false;
        }
        std::cout << "Noesis::GUI::SetXamlProvider(XamlProvider *) @ " << fn << std::endl;
        Noesis_GUI_SetXamlProvider = (void(__stdcall*)(XamlProvider*))fn;
    }

    {
        FARPROC fn = GetProcAddress(noesisDLL, "?LoadXaml@GUI@Noesis@@YA?AV?$Ptr@VBaseComponent@Noesis@@@2@PEBD@Z");
        if (fn == NULL) {
            std::cout << "Cannot find Noesis::GUI::LoadXaml(char *)" << std::endl;
            return false;
        }
        std::cout << "Noesis::GUI::LoadXaml(char *) @ " << fn << std::endl;
        Noesis_GUI_LoadXaml_String = (Noesis_Ptr<Noesis_BaseComponent>*(__stdcall*)(void*, char const*))fn;
    }

    std::cout << "DetourAttach(" << NoesisInit << ", " << MyNoesisInit << ")" << std::endl;
    if (auto result = DetourAttach((PVOID*)&NoesisInit, (PVOID)MyNoesisInit)) {
        std::cout << "DetourAttach() for Noesis::Init() returned " << result << std::endl;
        return false;
    }
    if (auto result = DetourAttach((PVOID*)&Noesis_GUI_SetXamlProvider, (PVOID)MyNoesis_GUI_SetXamlProvider)) {
        std::cout << "DetourAttach() for Noesis::GUI::SetXamlProvider(XamlProvider *) returned " << result << std::endl;
        return false;
    }
    if (auto result = DetourAttach((PVOID*)&Noesis_GUI_LoadXaml_String, (PVOID)MyNoesis_GUI_LoadXaml_String)) {
        std::cout << "DetourAttach() for Noesis::GUI::LoadXaml(char *) returned " << result << std::endl;
        return false;
    }
    std::cout << "DetourAttach() ok" << std::endl;

    if (auto result = DetourTransactionCommit()) {
        std::cout << "DetourTransactionCommit() returned " << result << std::endl;
        return false;
    }
    std::cout << "DetourTransactionCommit() ok" << std::endl;

    std::cout << "Mod initialized!" << std::endl;
    return true;
}
