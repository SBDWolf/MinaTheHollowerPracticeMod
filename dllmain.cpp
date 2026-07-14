#define _CRT_SECURE_NO_WARNINGS

#define SAVE_SLOT_MAX 999
#define MOD_NAME "PracticeMod"

#include <fstream>
#include <string>
#include <iostream>
#include <filesystem>
#include "MinaModAPI.h"
#include "MinaModEnums.h"

MinaModAPI* Mina = nullptr;
char* copiedSave = nullptr;
int saveSlot = 1;

void FixedUpdate(void*);
bool LoadSaveFromFile(const std::string& filePath);
bool SaveCurrentSlotToFile(const std::string& filePath);
static bool ExtractSaveSlotFromGameSave(const std::string& fullSaveData, int slotIndex, std::string& outSlotData);
static std::string GetEnvOrDefault(const char* var, const std::string& fallback);

extern "C"
__declspec(dllexport)
void MinaMod_Init(MinaModAPI* mm)
{
    Mina = mm;
    Mina->InstallHook("FixedUpdate", 0, FixedUpdate);
}

void FixedUpdate(void*)
{
    bool isCtrl = Mina->IsKeyHeld(YC_KEY_LCTRL) || Mina->IsKeyDown(YC_KEY_LCTRL) || Mina->IsKeyHeld(YC_KEY_RCTRL) || Mina->IsKeyDown(YC_KEY_RCTRL);
    bool isR2 = Mina->IsButtonHeld(YC_INPUT_R2) || Mina->IsButtonDown(YC_INPUT_R2);
    bool isL2 = Mina->IsButtonHeld(YC_INPUT_L2) || Mina->IsButtonDown(YC_INPUT_L2);
    
    if (Mina->IsKeyDown(YC_KEY_C) && isCtrl || Mina->IsButtonDown(YC_INPUT_R3) && isR2)
    {
        Mina->Log("[%s] Exporting save to file.\n", MOD_NAME);
        char buf[5];
        snprintf(buf, sizeof(buf), "%03d", saveSlot);
        bool ok = SaveCurrentSlotToFile(std::string("custom_save") + buf + ".sv");
        if (!ok) {
            Mina->Log("[%s] Save export failed.\n", MOD_NAME);
        }
        else {
            Mina->SoundPlay("bike_bell");
        }
    }
    if (Mina->IsKeyDown(YC_KEY_V) && isCtrl || Mina->IsButtonDown(YC_INPUT_L3) && isR2)
    {
        Mina->Log("[%s] Importing save from file.\n", MOD_NAME);
        char buf[5];
        snprintf(buf, sizeof(buf), "%03d", saveSlot);
        bool ok = LoadSaveFromFile(std::string("custom_save") + buf + ".sv");
        if (ok) {
            Mina->PlayerRestoreFromSave();
            Mina->StartActiveSaveSlot();
        }
        else {
            Mina->Log("[%s] Save import failed.\n", MOD_NAME);
        }
    }
    if (Mina->IsKeyDown(YC_KEY_B) && isCtrl || Mina->IsButtonDown(YC_INPUT_RSTICK_LEFT) && isR2)
    {
        if (saveSlot > 1) {
            Mina->SoundPlay("window_close");
            saveSlot -= 1;
        }
    }
    if (Mina->IsKeyDown(YC_KEY_N) && isCtrl || Mina->IsButtonDown(YC_INPUT_RSTICK_RIGHT) && isR2)
    {
        if (saveSlot < SAVE_SLOT_MAX) {
            Mina->SoundPlay("window_open");
            saveSlot += 1;
        }

    }

}


bool LoadSaveFromFile(const std::string& fileName)
{
    std::filesystem::path base;
    base = GetEnvOrDefault("APPDATA", "C:\\Users\\Default\\AppData\\Roaming");
    std::filesystem::path filePath = base / "Yacht Club Games" / "Mina the Hollower" / "mods" / MOD_NAME / "saves" / fileName;

    auto finalFilePath = std::filesystem::path(filePath).lexically_normal();

    if (!std::filesystem::exists(finalFilePath)) {
        Mina->Log("[%s] File not found.\n", MOD_NAME);
        return false;
    }

    std::ifstream file(finalFilePath, std::ios::binary);
    if (!file) {
        Mina->Log("[%s] Failed to open file from filepath.\n", MOD_NAME);
        return false;
    }

    std::string saveData((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    bool success = Mina->SetActiveSaveSlotContents(saveData.c_str());
    if (!success) {
        Mina->Log("[%s] API rejected save data.\n", MOD_NAME);
        return false;
    }
    Mina->Log("[%s] Loaded file successfully.\n", MOD_NAME);
    return true;
}

bool SaveCurrentSlotToFile(const std::string& fileName)
{
    std::filesystem::path base;
    base = GetEnvOrDefault("APPDATA", "C:\\Users\\Default\\AppData\\Roaming");

    std::filesystem::path gameSavePath = base / "Yacht Club Games" / "Mina the Hollower" / "saveData.yc";
    if (!std::filesystem::exists(gameSavePath)) {
        Mina->Log("[%s] Game save file not found.\n", MOD_NAME);
        return false;
    }

    std::filesystem::path modSaveDir = base / "Yacht Club Games" / "Mina the Hollower" / "mods" / MOD_NAME / "saves";
    std::filesystem::path outFilePath = modSaveDir / fileName;


    auto finalOutFilePath = std::filesystem::path(outFilePath).lexically_normal();

    if (!std::filesystem::exists(modSaveDir)) {
        if (!std::filesystem::create_directories(modSaveDir)) {
            Mina->Log("[%s] Failed to create directory.\n", MOD_NAME);
            return false;
        }
    }

    std::ifstream gameFile(gameSavePath, std::ios::binary);
    if (!gameFile) {
        Mina->Log("[%s] Failed to open game save file.\n", MOD_NAME);
        return false;
    }
    std::string gameSaveData((std::istreambuf_iterator<char>(gameFile)), std::istreambuf_iterator<char>());
    gameFile.close();

    int activeSlot = Mina->GetActiveSaveSlot();
    std::string extractedSlot;
    if (!ExtractSaveSlotFromGameSave(gameSaveData, activeSlot, extractedSlot)) {
        Mina->Log("[%s] Failed to extract slot %d from saveData.yc\n", MOD_NAME, activeSlot);
        return false;
    }

    std::string saveData = "[YCD Version: 1]\nSaveSlot\n" + extractedSlot;

    std::ofstream file(finalOutFilePath, std::ios::out | std::ios::trunc);
    if (!file) {
        Mina->Log("[%s] Failed to open file for writing.\n", MOD_NAME);
        return false;
    }

    file.write(saveData.data(), saveData.size());
    file.close();

    if (!file.good()) {
        Mina->Log("[%s] Write failed. \n", MOD_NAME);
        return false;
    }

    Mina->Log("[%s] Saved file.\n", MOD_NAME);
    return true;
}

static bool ExtractSaveSlotFromGameSave(const std::string& fullSaveData, int slotIndex, std::string& outSlotData)
{
    size_t startPos = fullSaveData.find("m_rSlots: [");
    if (startPos == std::string::npos) {
        return false;
    }

    size_t cursor = startPos;
    int foundSlots = 0;
    size_t dataLength = fullSaveData.length();

    while (cursor < dataLength)
    {
        size_t slotKeyPos = fullSaveData.find("SaveSlot", cursor);
        if (slotKeyPos == std::string::npos) {
            break;
        }

        size_t bracePos = fullSaveData.find('{', slotKeyPos);
        if (bracePos == std::string::npos || bracePos >= dataLength) {
            break;
        }

        // find matching closing brace
        int depth = 0;
        size_t i = bracePos;
        while (i < dataLength)
        {
            char c = fullSaveData[i];
            if (c == '{') {
                depth++;
            }
            else if (c == '}') {
                depth--;
            }

            if (depth == 0) {
                break;
            }
            i++;
        }

        if (depth != 0) {
            break;
        }
        size_t slotEnd = i;

        if (foundSlots == slotIndex)
        {
            outSlotData = fullSaveData.substr(bracePos, slotEnd - bracePos + 1);
            return true;
        }

        foundSlots++;
        cursor = slotEnd + 1;
    }
    return false;
}


static std::string GetEnvOrDefault(const char* var, const std::string& fallback)
{
    const char* val = std::getenv(var);
    return val ? std::string(val) : fallback;
}