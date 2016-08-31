#include "fs.h"
#include "draw.h"
#include "platform.h"
#include "decryptor/aes.h"
#include "decryptor/decryptor.h"
#include "decryptor/keys.h"
#include "decryptor/nand.h"
#include "decryptor/nandfat.h"
#include "decryptor/titlekey.h"

u32 CryptTitlekey(TitleKeyEntry* entry, bool encrypt)
{
    CryptBufferInfo info = {.keyslot = 0x3D, .setKeyY = 0, .size = 16, .buffer = entry->titleKey,
        .mode = encrypt ? AES_CNT_TITLEKEY_ENCRYPT_MODE : AES_CNT_TITLEKEY_DECRYPT_MODE};
    memset(info.ctr, 0, 16);
    memcpy(info.ctr, entry->titleId, 8);
    SetupCommonKeyY0x3D(entry->commonKeyIndex);
    
    CryptBuffer(&info);
    
    return 0;
}

u32 CryptTitlekeysFile(u32 param)
{
    TitleKeysInfo *info = (TitleKeysInfo*)0x20316000;
    char filename[64];

    if (InputFileNameSelector(filename, (param & TK_ENCRYPTED) ? "decTitleKeys.bin" : "encTitleKeys.bin",
        NULL, NULL, 0, 16, true) != 0)
        return 1;
    
    if (!DebugFileOpen(filename))
        return 1;
    if (!DebugFileRead(info, 16, 0)) {
        FileClose();
        return 1;
    }
    if (!info->n_entries || info->n_entries > MAX_ENTRIES) {
        Debug("Too many/few entries specified: %i", info->n_entries);
        FileClose();
        return 1;
    }
    Debug("Number of entries: %i", info->n_entries);
    if (!DebugFileRead(info->entries, info->n_entries * sizeof(TitleKeyEntry), 16)) {
        FileClose();
        return 1;
    }
    FileClose();

    Debug("%scrypting Title Keys...", (param & TK_ENCRYPTED) ? "En" : "De");
    for (u32 i = 0; i < info->n_entries; i++)
        CryptTitlekey(&(info->entries[i]), (param & TK_ENCRYPTED));

    if (OutputFileNameSelector(filename, (param & TK_ENCRYPTED) ? "encTitleKeys.bin" : "decTitleKeys.bin", NULL) != 0)
        return 1;
    u32 out_size = info->n_entries * sizeof(TitleKeyEntry) + 16;
    if (FileDumpData(filename, info, out_size) != out_size) {
        Debug("Error writing Titlekeys file");
        return 1;
    }
    

    return 0;
}

u32 DumpTitlekeysNand(u32 param)
{
    PartitionInfo* ctrnand_info = GetPartitionInfo(P_CTRNAND);;
    u8* buffer = BUFFER_ADDRESS;
    TitleKeysInfo *info = (TitleKeysInfo*) 0x20316000;
    char filename[64];
    
    u32 nKeys = 0;
    u32 nSkipped = 0;
    u32 offset = 0;
    u32 size = 0;
    
    if (DebugSeekFileInNand(&offset, &size, "ticket.db", "DBS        TICKET  DB ", ctrnand_info) != 0)
        return 1;
    
    Debug("%s Title Keys...", (param & TK_ENCRYPTED) ? "Dumping" : "Decrypting");
    memset(info, 0, 0x10);
    for (u32 t_offset = 0; t_offset < size; t_offset += BUFFER_MAX_SIZE - NAND_SECTOR_SIZE) {
        u32 read_bytes = min(BUFFER_MAX_SIZE, (size - t_offset));
        ShowProgress(t_offset, size);
        if (DecryptNandToMem(buffer, offset + t_offset, read_bytes, ctrnand_info) != 0)
            return 1;
        for (u32 i = 0; i < read_bytes - NAND_SECTOR_SIZE; i++) {
            if(memcmp(buffer + i, (u8*) "Root-CA00000003-XS0000000c", 26) == 0) {
                u32 exid;
                u32 consoleId = getle32(buffer + i + 0x98);
                u8* titleId = buffer + i + 0x9C;
                u32 commonKeyIndex = *(buffer + i + 0xB1);
                u8* titlekey = buffer + i + 0x7F;
                for (exid = 0; exid < nKeys; exid++)
                    if (memcmp(titleId, info->entries[exid].titleId, 8) == 0)
                        break;
                if (!consoleId || (exid < nKeys)) {
                    nSkipped++;
                    continue; // skip useless / duplicates
                }
                memset(&(info->entries[nKeys]), 0, sizeof(TitleKeyEntry));
                memcpy(info->entries[nKeys].titleId, titleId, 8);
                memcpy(info->entries[nKeys].titleKey, titlekey, 16);
                info->entries[nKeys].commonKeyIndex = commonKeyIndex;
                if (!(param & TK_ENCRYPTED))
                    CryptTitlekey(&(info->entries[nKeys]), false);
                nKeys++;
            }
        }
        if (nKeys == MAX_ENTRIES) {
            Debug("Maximum number of titlekeys found");
            break;
        }
    }
    info->n_entries = nKeys;
    ShowProgress(0, 0);
    
    Debug("%s %u unique Titlekeys", (param & TK_ENCRYPTED) ? "Dumped" : "Decrypted", nKeys);
    Debug("Skipped %u useless Titlekeys", nSkipped);
    
    if (OutputFileNameSelector(filename, (param & TK_ENCRYPTED) ? "encTitleKeys.bin" : "decTitleKeys.bin", NULL) != 0)
        return 1;
    
    if(nKeys > 0) {
        u32 out_size = info->n_entries * sizeof(TitleKeyEntry) + 16;
        if (FileDumpData(filename, info, out_size) != out_size) {
            Debug("Error writing Titlekeys file");
            return 1;
        }
    } else {
        return 1;
    }

    return 0;
}
