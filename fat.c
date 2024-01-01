#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#pragma region Constants
#define TRUE 1
#define FALSE 0

#define ENTRY_SIZE 32

#define FAT12_16_BORDER 4086
#define FAT16_32_BORDER 65526

#define CLUSTER_START 2
#define FAT12_CLUSTER_END 0xFF6
#define FAT16_CLUSTER_END 0xFFF6
#define FAT32_CLUSTER_END 0x0FFFFFF6

#define MAX_NAME_LENGTH 256

// エントリの先頭のバイト
#define SKIPPED 0
#define DELETED 0xe5
#define ESCAPE_DELETED 0x05
#define FIRST_ENTRY_OF_LONG_NAME 0x40

// 属性ビット
#define READ_ONLY 0x01
#define HIDDEN 0x02
#define SYSTEM 0x04
#define VOLUME_ID 0x08
#define DIRECTORY 0x10
#define ARCHIVE 0x20
#define LONG_NAME 0x0f

// パスの区切り文字
#define PATH_DELIMITER L"/"
#pragma endregion

#pragma region Integer types
// 8ビットの符号あり整数
typedef char s8;

// 16ビットの符号あり整数
typedef short s16;

// 32ビットの符号あり整数
typedef int s32;

// 64ビットの符号あり整数
typedef long long s64;

// 8ビットの符号なし整数
typedef unsigned char u8;

// 16ビットの符号なし整数
typedef unsigned short u16;

// 32ビットの符号なし整数
typedef unsigned int u32;

// 64ビットの符号なし整数
typedef unsigned long long u64;
#pragma endregion

#pragma region Alias types
// ブール値
typedef int Boolean;

// 処理の結果
typedef int Result;
#pragma endregion

#pragma region String utilities
/**
 * char[]をwchar_t[]に変換する
 * 変換後の文字列の長さを返す
 */
s32 toWide(wchar_t **dist, const char *src)
{
    *dist = NULL;

    // 必要なバッファサイズを計算する
    s32 bufferSize = mbstowcs(NULL, src, 0);
    if (bufferSize < 0)
    {
        return -1;
    }

    // バッファを確保する
    *dist = malloc((bufferSize + 1) * sizeof(wchar_t));
    if (*dist == NULL)
    {
        return -2;
    }

    // 変換する
    s32 size = mbstowcs(*dist, src, bufferSize + 1);
    if (size < 0)
    {
        free(*dist);
        return -3;
    }

    return size;
}

/**
 * 文字列をコピーする
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result coptString(wchar_t **copyPointer, const wchar_t *base)
{
    s32 length = wcslen(base);
    wchar_t *copy = *copyPointer = calloc((length + 1), sizeof(wchar_t));
    if (copy == NULL)
    {
        return 1;
    }
    wcscpy(copy, base);
    return 0;
}

/**
 * 文字列を実際の長さに切り詰める
 * 切り詰めた後の文字列の長さを返す
 */
s32 shortenString(wchar_t **string)
{
    s32 length = wcslen(*string);
    wchar_t *newString = realloc(*string, (length + 1) * sizeof(wchar_t));
    if (newString == NULL)
    {
        return -1;
    }
    *string = newString;
    return length;
}

// 文字列の順番を反転する
void reverseString(wchar_t *string, s32 length)
{
    for (s32 i = 0, j = length - 1; i < j; ++i, --j)
    {
        // 文字を交換する
        wchar_t temp = string[i];
        string[i] = string[j];
        string[j] = temp;
    }
}

// 文字列から末尾の空白を除外する
void trimEnd(wchar_t *string, s32 length)
{
    if (length <= 0)
    {
        return;
    }

    // 末尾から空白と空白以外の文字の境目を探す
    s32 index = length - 1;
    for (; index >= 0; --index)
    {
        if (string[index] != ' ')
        {
            break;
        }
    }

    // 境目以降の空白を切り捨てる
    if (index + 1 < length)
    {
        string[index + 1] = '\0';
    }
}

// 指定された文字列が、指定された接頭辞で始まるかどうかを判定する
Boolean startsWith(const wchar_t *string, const wchar_t *prefix)
{
    return wcsncmp(string, prefix, wcslen(prefix)) == 0;
}

// ファイル名を表す文字列から拡張子を除いたファイル名と拡張子を取り出す
void getBasenameAndExtension(const wchar_t *string, wchar_t *basename, wchar_t *extension)
{
    s32 length = wcslen(string);
    s32 index = length - 1;

    // 末尾からドットを探す
    for (; index >= 0; --index)
    {
        if (string[index] == '.')
        {
            break;
        }
    }

    // ファイル名から拡張子を除いた部分をコピーする
    wcsncpy(basename, string, index);
    basename[index < 0 ? 0 : index] = '\0';

    // 拡張子の部分をコピーする
    wcscpy(extension, string + index + 1);
    extension[length - index - 1] = '\0';
}
#pragma endregion

#pragma region Datetime
// 日時を表す
typedef struct __Datetime
{
    // 西暦
    u16 year;

    // 月
    u8 month;

    // 日
    u8 dayOfMonth;

    // 時
    u8 hour;

    // 分
    u8 minute;

    // 秒
    u8 second;

    // ミリ秒
    u16 millisecond;
} Datetime;

/**
 * 日時を表す整数でDatetimeを作成する
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result createDatetime(Datetime **datetimePointer, u16 date, u16 time, u8 tenth)
{
    Datetime *datetime = *datetimePointer = malloc(sizeof(Datetime));
    if (datetime == NULL)
    {
        return 1;
    }

    datetime->year = ((date & 0xfe00) >> 9) + 1980;
    datetime->month = (date & 0x1e0) >> 5;
    datetime->dayOfMonth = date & 0x1f;
    datetime->hour = (time & 0xf800) >> 11;
    datetime->minute = (time & 0x7e0) >> 5;
    datetime->second = (time & 0x1f) << 1 + tenth / 100;
    datetime->millisecond = tenth * 100 % 10000;

    return 0;
}

/**
 * Datetimeをコピーする
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result copyDatetime(Datetime **copyPointer, Datetime *base)
{
    Datetime *copy = *copyPointer = malloc(sizeof(Datetime));
    if (copy == NULL)
    {
        return 1;
    }

    memcpy(copy, base, sizeof(Datetime));

    return 0;
}
#pragma endregion

#pragma region Image
typedef struct __Image Image;
typedef struct __Entry Entry;
typedef struct __File File;

// FATのサブタイプを表す
typedef enum __FATType
{
    FAT12,
    FAT16,
    FAT32,
} FATType;

// FATイメージを表す
typedef struct __Image
{
    // FATイメージを表すファイルのポインタ
    FILE *fp;

    /**
     * FATイメージの中で開いているエントリ
     * エントリを要素として連結リストで管理する
     */
    Entry *openedEntry;

    // 1セクタあたりのバイト数
    u16 sectorSize;

    // 1クラスタあたりのバイト数
    u32 clusterSize;

    // FAT領域のオフセット
    u32 fatOffset;

    // ルートディレクトリ領域のオフセット
    u64 rootOffset;

    // データ領域のオフセット
    u64 dataOffset;

    // ルートディレクトリのクラスタ番号
    u32 rootCluster;

    // ルートディレクトリの最大エントリ数
    u32 maxRootEntryCount;

    // ルートディレクトリ以外の最大エントリ数
    u32 maxSubEntryCount;

    // FATのサブタイプ
    FATType fatType;

    // 使用中のクラスタ番号のうち最大のもの
    u32 clusterEnd;

    // FATから次のクラスタ番号を取得する
    u32 (*getNextCluster)(const Image *image, u32 cluster);
} Image;

// FATイメージに含まれるエントリを表す
typedef struct __Entry
{
    // エントリが含まれるFATイメージ
    Image *image;

    // 次の開いているエントリ
    Entry *nextOpenedEntry;

    /**
     * エントリを指すファイルのポインタ
     * ポインタを要素として連結リストで管理する
     */
    File *openedFile;

    // エントリの名前
    wchar_t *name;

    // 読み取り専用かどうか
    Boolean readonly;

    // 非表示かどうか
    Boolean hidden;

    // システム関連のエントリかどうか
    Boolean system;

    // ボリュームかどうか
    Boolean volume;

    // ディレクトリかどうか
    Boolean directory;

    // ファイルかどうか
    Boolean file;

    // 作成日時
    Datetime *createdAt;

    // 更新日時
    Datetime *modifiedAt;

    // アクセス日時
    Datetime *accessedAt;

    // エントリのサイズ
    u32 size;

    // エントリの最初のクラスタ番号
    u32 cluster;
} Entry;

// FATイメージのファイルのエントリのポインタを表す
typedef struct __File
{
    // ポインタが指すエントリ
    Entry *entry;

    // 次の開いているポインタ
    File *nextOpenedFile;

    // ファイルの中の位置
    u32 position;

    // 現在のクラスタ番号
    u32 cluster;
} File;

// バイト列から指定したオフセットの8ビットを取得する
u8 get8(const u8 *bytes, u8 offset)
{
    u8 v0 = bytes[offset];
    return v0;
}

// バイト列から指定したオフセットの16ビットを取得する
u16 get16(const u8 *bytes, u8 offset)
{
    u16 v0 = bytes[offset];
    u16 v1 = bytes[offset + 1];
    return v0 | (v1 << 8);
}

// バイト列から指定したオフセットの32ビットを取得する
u32 get32(const u8 *bytes, u8 offset)
{
    u32 v0 = bytes[offset];
    u32 v1 = bytes[offset + 1];
    u32 v2 = bytes[offset + 2];
    u32 v3 = bytes[offset + 3];
    return v0 | (v1 << 8) | (v2 << 16) | (v3 << 24);
}

void closeEntry(Entry *entry);
void closeFile(File *file);

u32 getNextCluster12(const Image *image, u32 cluster);
u32 getNextCluster16(const Image *image, u32 cluster);
u32 getNextCluster32(const Image *image, u32 cluster);

/**
 * FATイメージを指定されたパスから開く
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result openImage(Image **imagePointer, const char *path)
{
    // FATイメージの領域を確保する
    Image *image = *imagePointer = malloc(sizeof(Image));
    if (image == NULL)
    {
        return 1;
    }

    // FATイメージを表すファイルを開く
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return 2;
    }

    // MBRを読み込む
    u8 bytes[64];
    fread(bytes, sizeof(u8), sizeof(bytes), fp);

    // メンバを初期化する
    image->fp = fp;
    image->openedEntry = NULL;

    u16 bytePerSector = get16(bytes, 11);
    image->sectorSize = bytePerSector;

    u8 sectorPerCluster = get8(bytes, 13);
    image->clusterSize = bytePerSector * sectorPerCluster;

    u16 reservedSectorCount = get16(bytes, 14);
    image->fatOffset = bytePerSector * reservedSectorCount;

    u8 fatCount = get8(bytes, 16);
    u32 fatSectorCount = get16(bytes, 22);
    image->rootOffset = image->fatOffset + bytePerSector * fatSectorCount * fatCount;

    u16 rootEntryCount = get16(bytes, 17);
    u64 dataOffset = image->rootOffset + ENTRY_SIZE * rootEntryCount;
    image->dataOffset = dataOffset - 2 * image->clusterSize;

    image->rootCluster = 0;
    image->maxRootEntryCount = rootEntryCount;
    image->maxSubEntryCount = image->clusterSize / ENTRY_SIZE;

    // データ領域の総クラスタ数からFATのサブタイプを決定する
    u32 totalSectorCount = get16(bytes, 19);
    if (totalSectorCount == 0)
    {
        totalSectorCount = get32(bytes, 32);
    }

    u32 dataSectorCount = totalSectorCount - dataOffset / bytePerSector;
    u32 dataClusterCount = dataSectorCount / sectorPerCluster;

    if (dataClusterCount < FAT12_16_BORDER)
    {
        image->fatType = FAT12;
        image->clusterEnd = FAT12_CLUSTER_END;
        image->getNextCluster = &getNextCluster12;
    }
    else if (dataClusterCount < FAT16_32_BORDER)
    {
        image->fatType = FAT16;
        image->clusterEnd = FAT16_CLUSTER_END;
        image->getNextCluster = &getNextCluster16;
    }
    else
    {
        image->fatType = FAT32;
        image->clusterEnd = FAT32_CLUSTER_END;
        image->getNextCluster = &getNextCluster32;
    }

    if (image->fatType == FAT32)
    {
        fatSectorCount = get32(bytes, 36);
        dataOffset = image->fatOffset + bytePerSector * fatSectorCount * fatCount;
        image->dataOffset = dataOffset - 2 * image->clusterSize;

        u32 rootCluster = get32(bytes, 44);
        image->rootCluster = rootCluster;
        image->rootOffset = image->dataOffset + image->clusterSize * rootCluster;

        image->maxRootEntryCount = image->maxSubEntryCount;
    }

    return 0;
}

/**
 * FATイメージを閉じる
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result closeImage(Image *image)
{
    Result result = fclose(image->fp);

    // 開いているエントリをすべて閉じる
    Entry *openedEntry = image->openedEntry;
    while (openedEntry != NULL)
    {
        Entry *nextOpenedEntry = openedEntry->nextOpenedEntry;
        closeEntry(openedEntry);
        openedEntry = nextOpenedEntry;
    }

    free(image);
    return result;
}

// クラスタ番号からデータ領域のオフセットを計算する
u64 getDataOffset(const Image *image, u32 cluster)
{
    return image->dataOffset + image->clusterSize * cluster;
}

/**
 * 指定されたクラスタの領域の先頭まで、FATイメージのファイルポインタを移動させる
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result seekTo(const Image *image, u32 cluster)
{
    u64 offset = getDataOffset(image, cluster);
    return fseek(image->fp, offset, SEEK_SET);
}

// 12ビットのFATから次のクラスタ番号を取得する
u32 getNextCluster12(const Image *image, u32 cluster)
{
    // FAT領域のクラスタ番号が表す部分の値を3バイト分取得する
    u32 offset = image->fatOffset + cluster / 2 * 3;
    fseek(image->fp, offset, SEEK_SET);
    u32 v0 = fgetc(image->fp);
    u32 v1 = fgetc(image->fp);
    u32 v2 = fgetc(image->fp);

    u16 v;
    if (cluster % 2 == 0)
    {
        // クラスタ番号が偶数の場合は下位12ビットを取得する
        v = (v0 | (v1 << 8));
    }
    else
    {
        // クラスタ番号が奇数の場合は上位12ビットを取得する
        v = ((v1 >> 4) | (v2 << 4));
    }
    return v & 0xfff;
}

// 16ビットのFATから次のクラスタ番号を取得する
u32 getNextCluster16(const Image *image, u32 cluster)
{
    u32 offset = image->fatOffset + cluster * 2;
    fseek(image->fp, offset, SEEK_SET);
    u32 v0 = fgetc(image->fp);
    u32 v1 = fgetc(image->fp);
    return v0 | (v1 << 8);
}

// 32ビットのFATから次のクラスタ番号を取得する
u32 getNextCluster32(const Image *image, u32 cluster)
{
    u32 offset = image->fatOffset + cluster * 4;
    fseek(image->fp, offset, SEEK_SET);
    u32 v0 = fgetc(image->fp);
    u32 v1 = fgetc(image->fp);
    u32 v2 = fgetc(image->fp);
    u32 v3 = fgetc(image->fp);
    u32 v = v0 | (v1 << 8) | (v2 << 16) | (v3 << 24);
    return v & 0x0FFFFFFF;
}
#pragma endregion

#pragma region Entry
u16 getChildren(Entry **children[], const Entry *parent);

/**
 * 指定されたイメージ、名前、バイト列でエントリを作成する
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result __openEntry(Entry **entryPointer, Image *image, wchar_t *name, const u8 *bytes)
{
    // エントリの領域を確保する
    Entry *entry = *entryPointer = malloc(sizeof(Entry));
    if (entry == NULL)
    {
        return 1;
    }

    // メンバを初期化する
    entry->image = image;
    entry->nextOpenedEntry = image->openedEntry;
    image->openedEntry = entry;

    entry->openedFile = NULL;

    entry->name = name;

    entry->readonly = (get8(bytes, 11) & READ_ONLY) != 0;
    entry->hidden = (get8(bytes, 11) & HIDDEN) != 0;
    entry->system = (get8(bytes, 11) & SYSTEM) != 0;
    entry->volume = (get8(bytes, 11) & VOLUME_ID) != 0;
    entry->directory = (get8(bytes, 11) & DIRECTORY) != 0;
    entry->file = (get8(bytes, 11) & ARCHIVE) != 0;

    createDatetime(&entry->createdAt, get16(bytes, 16), get16(bytes, 14), get8(bytes, 13));
    createDatetime(&entry->modifiedAt, get16(bytes, 24), get16(bytes, 22), 0);
    createDatetime(&entry->accessedAt, get16(bytes, 18), 0, 0);

    entry->size = get32(bytes, 28);

    u32 higherCluster = get16(bytes, 20);
    u32 lowerCluster = get16(bytes, 26);
    entry->cluster = (higherCluster << 16) | lowerCluster;

    return 0;
}

/**
 * 指定されたエントリをコピーする
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result copyEntry(Entry **copyPointer, const Entry *base)
{
    // エントリの領域を確保する
    Entry *copy = *copyPointer = malloc(sizeof(Entry));
    if (copy == NULL)
    {
        return 1;
    }

    memcpy(copy, base, sizeof(Entry));

    copy->nextOpenedEntry = base->image->openedEntry;
    base->image->openedEntry = copy;

    copy->openedFile = NULL;

    coptString(&copy->name, base->name);

    copyDatetime(&copy->createdAt, base->createdAt);
    copyDatetime(&copy->modifiedAt, base->modifiedAt);
    copyDatetime(&copy->accessedAt, base->accessedAt);

    return 0;
}

/**
 * 指定された名前の子エントリを取得する
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result getChildEntry(Entry **childPointer, const Entry *parent, const wchar_t *name)
{
    *childPointer = NULL;

    Entry **children;
    u16 count = getChildren(&children, parent);
    if (count < 0)
    {
        return 1;
    }

    Entry *child = NULL;

    // 子エントリのうち、パスの一部と名前が一致するエントリを探す
    for (u16 i = 0; i < count; ++i)
    {
        if (wcscmp(name, children[i]->name) == 0)
        {
            child = children[i];
            break;
        }
    }

    // 見つけたエントリ以外を閉じる
    for (s32 i = count - 1; i >= 0; --i)
    {
        if (children[i] != child)
        {
            closeEntry(children[i]);
        }
    }

    free(children);

    // 名前が一致するエントリが見つからなかったら
    if (child == NULL)
    {
        return 127;
    }

    *childPointer = child;
    return 0;
}

/**
 * 指定されたパスの子孫エントリを取得する
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result getDescendantEntry(Entry **descendantPointer, const Entry *parent, const wchar_t *path)
{
    *descendantPointer = NULL;

    // 親のエントリをコピーする
    Entry *descendant;
    Result result = copyEntry(&descendant, parent);
    if (result)
    {
        return result;
    }

    // パスをコピーする
    wchar_t *pathCopy;
    result = coptString(&pathCopy, path);
    if (result)
    {
        return result;
    }

    // パスを区切り文字で分割してエントリを探していく
    wchar_t *savePointer;
    wchar_t *token = wcstok(pathCopy, PATH_DELIMITER, &savePointer);

    while (token != NULL)
    {
        Entry *child;
        result = getChildEntry(&child, descendant, token);
        closeEntry(descendant);

        if (result)
        {
            break;
        }

        token = wcstok(NULL, PATH_DELIMITER, &savePointer);
        descendant = child;
    }

    if (result == 0)
    {
        *descendantPointer = descendant;
    }

    free(pathCopy);
    return result;
}

/**
 * 指定されたFATイメージ、パスのエントリを開く
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result openEntry(Entry **entryPointer, Image *image, const wchar_t *path)
{
    *entryPointer = NULL;

    // ルートのエントリを作成する
    Entry *root;
    wchar_t *name = malloc(sizeof(wchar_t));
    wcscpy(name, L"");
    u8 bytes[ENTRY_SIZE] = {0};
    bytes[11] = DIRECTORY;
    bytes[20] = (image->rootCluster >> 16) & 0xff;
    bytes[21] = (image->rootCluster >> 24) & 0xff;
    bytes[26] = image->rootCluster & 0xff;
    bytes[27] = (image->rootCluster >> 8) & 0xff;
    Result result = __openEntry(&root, image, name, bytes);
    if (result)
    {
        return result;
    }

    return getDescendantEntry(entryPointer, root, path);
}

// エントリを閉じる
void closeEntry(Entry *entry)
{
    // FATイメージの開いているエントリからエントリを除外する
    if (entry->image->openedEntry == entry)
    {
        entry->image->openedEntry = entry->nextOpenedEntry;
    }
    else
    {
        Entry *prevOpenedEntry = entry->image->openedEntry;
        while (prevOpenedEntry->nextOpenedEntry != entry)
        {
            prevOpenedEntry = prevOpenedEntry->nextOpenedEntry;
        }
        prevOpenedEntry->nextOpenedEntry = entry->nextOpenedEntry;
    }

    // 開いているポインタをすべて閉じる
    File *openedFile = entry->openedFile;
    while (openedFile != NULL)
    {
        File *nextOpenedFile = openedFile->nextOpenedFile;
        closeFile(openedFile);
        openedFile = nextOpenedFile;
    }

    free(entry->name);
    free(entry->createdAt);
    free(entry->modifiedAt);
    free(entry->accessedAt);

    free(entry);
}

// 指定されたエントリがルートかどうかを返す
Boolean getIsRoot(const Entry *entry)
{
    return entry->cluster == entry->image->rootCluster;
}

/**
 * 指定されたディレクトリのエントリの子エントリを取得する
 * 実際に取得された子エントリの数を返す
 */
u16 getChildren(Entry **childrenPointer[], const Entry *parent)
{
    *childrenPointer = NULL;

    if (!parent->directory)
    {
        return -1;
    }

    u32 cluster = parent->cluster;
    u32 maxEntryCount;

    if (getIsRoot(parent))
    {
        fseek(parent->image->fp, parent->image->rootOffset, SEEK_SET);
        maxEntryCount = parent->image->maxRootEntryCount;
    }
    else
    {
        seekTo(parent->image, cluster);
        maxEntryCount = parent->image->maxSubEntryCount;
    }

    // 読み込んだバイト列
    u8 bytes[ENTRY_SIZE];

    // エントリの名前
    wchar_t *name = calloc(MAX_NAME_LENGTH, sizeof(wchar_t));

    // 列挙されたエントリの個数
    u16 count = 0;

    while (TRUE)
    {
        for (u32 i = 0; i < maxEntryCount; ++i)
        {
            // バイト列を読み込む
            fread(bytes, sizeof(u8), ENTRY_SIZE, parent->image->fp);

            if (bytes[0] == SKIPPED)
            {
                break;
            }

            if (bytes[0] == DELETED)
            {
                continue;
            }

            if (bytes[0] == ESCAPE_DELETED)
            {
                bytes[0] = DELETED;
            }

            if (bytes[11] == LONG_NAME)
            {
                // 読み込んだバイト列が長い名前の一部であれば、それが表す名前をエントリの名前に逆順で追加する
                wchar_t subName[14];
                subName[13] = '\0';

                for (u8 j = 0; j < 2; ++j)
                {
                    subName[j] = get16(bytes, 30 - j * 2);
                }

                for (u8 j = 0; j < 6; ++j)
                {
                    subName[2 + j] = get16(bytes, 24 - j * 2);
                }

                for (u8 j = 0; j < 5; ++j)
                {
                    subName[8 + j] = get16(bytes, 9 - j * 2);
                }

                // 長い名前の最初のエントリの場合、末尾まで空白で埋める
                if ((bytes[0] & FIRST_ENTRY_OF_LONG_NAME) != 0)
                {
                    u8 index = 0;
                    for (; subName[index] != '\0'; ++index)
                    {
                        subName[index] = ' ';
                    }
                    subName[index] = ' ';
                }

                wcscat(name, subName);
            }
            else
            {
                if (wcslen(name) == 0)
                {
                    // エントリが短い名前だったら

                    // 拡張子を除いた名前を取得する
                    wchar_t basename[9];
                    basename[8] = '\0';
                    for (u8 j = 0; j < 8; ++j)
                    {
                        basename[j] = bytes[j];
                    }
                    trimEnd(basename, 8);
                    wcscat(name, basename);

                    if ((bytes[11] & ARCHIVE) != 0)
                    {
                        wcscat(name, L".");
                    }

                    // 拡張子を取得する
                    wchar_t extension[4];
                    extension[3] = '\0';
                    for (u8 j = 0; j < 3; ++j)
                    {
                        extension[j] = bytes[8 + j];
                    }
                    trimEnd(extension, 3);
                    wcscat(name, extension);

                    // エントリの名前を実際の長さに切り詰める
                    s32 length = shortenString(&name);
                    if (length < 0)
                    {
                        break;
                    }
                }
                else
                {
                    // エントリが長い名前だったら

                    // エントリの名前を実際の長さに切り詰める
                    s32 length = shortenString(&name);
                    if (length < 0)
                    {
                        break;
                    }

                    // 逆順でつなげたエントリの名前の順番を反転する
                    reverseString(name, length);
                    trimEnd(name, length);
                }

                Entry *child;
                Result result = __openEntry(&child, parent->image, name, bytes);
                if (result)
                {
                    break;
                }

                count++;

                // 新たなエントリの名前の領域を確保する
                name = calloc(MAX_NAME_LENGTH, sizeof(wchar_t));
            }
        }

        // 最後のクラスタに到達していたらループを抜け出す
        if (cluster < CLUSTER_START || cluster > parent->image->clusterEnd)
        {
            break;
        }

        cluster = parent->image->getNextCluster(parent->image, cluster);
        seekTo(parent->image, cluster);
    }

    free(name);

    // FATイメージの開いているエントリのリストから子エントリを取得する。
    Entry **children = *childrenPointer = malloc(count * sizeof(Entry *));
    Entry *child = parent->image->openedEntry;
    for (s32 i = count - 1; i >= 0; --i)
    {
        children[i] = child;
        child = child->nextOpenedEntry;
    }

    return count;
}
#pragma endregion

#pragma region File
/**
 * 指定されたエントリのポインタを開く
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result openFile(File **filePointer, Entry *entry)
{
    *filePointer = NULL;

    if (!entry->file)
    {
        return 1;
    }

    // ポインタの領域を確保する
    File *file = *filePointer = malloc(sizeof(File));
    if (file == NULL)
    {
        return 2;
    }

    // メンバを初期化する
    file->entry = entry;
    file->nextOpenedFile = entry->openedFile;
    entry->openedFile = file;

    file->position = 0;
    file->cluster = entry->cluster;

    return 0;
}

// ポインタを閉じる
void closeFile(File *file)
{
    // エントリの開いているポインタからポインタを除外する
    if (file->entry->openedFile == file)
    {
        file->entry->openedFile = file->nextOpenedFile;
    }
    else
    {
        File *prevOpenedFile = file->entry->openedFile;
        while (prevOpenedFile->nextOpenedFile != file)
        {
            prevOpenedFile = prevOpenedFile->nextOpenedFile;
        }
        prevOpenedFile->nextOpenedFile = file->nextOpenedFile;
    }

    free(file);
}

/**
 * 指定されたポインタから、指定された長さのバイト列を読み込む
 * 実際に読み込まれたバイト列の長さを返す
 */
u32 readFile(u8 *bytes, u32 size, File *file)
{
    if (size == 0)
    {
        return 0;
    }

    // ポインタがファイルの終端に達していたら
    if (file->position == file->entry->size)
    {
        return 0;
    }

    // FATイメージのファイルポインタの位置をポインタに合わせる
    u64 dataOffset = getDataOffset(file->entry->image, file->cluster);
    u32 positionOffset = file->position % file->entry->image->clusterSize;
    u64 fileOffset = dataOffset + positionOffset;
    if (ftell(file->entry->image->fp) != fileOffset)
    {
        fseek(file->entry->image->fp, fileOffset, SEEK_SET);
    }

    // 指定されたバイト列の長さがポインタの読み込める範囲を超えていたら、範囲に収まるようにする
    s64 surplus = (s64)(file->position + size) - file->entry->size;
    if (surplus > 0)
    {
        size -= surplus;
    }

    // クラスタごとにバイト列を読み込む
    u32 newPositionOffset = positionOffset + size;
    u32 clusterCount = newPositionOffset / file->entry->image->clusterSize;
    if (clusterCount == 0)
    {
        // 読み込む範囲が現在のクラスタに収まっている場合
        fread(bytes, sizeof(u8), size, file->entry->image->fp);
    }
    else
    {
        // 読み込む範囲が複数のクラスタにまたがっている場合

        // 現在のクラスタの残りのバイト列を読み込む
        u32 readSize = file->entry->image->clusterSize - positionOffset;
        fread(bytes, sizeof(u8), readSize, file->entry->image->fp);
        bytes += readSize;

        // 読み込む範囲に完全に含まれているクラスタからバイト列を読み込む
        readSize = file->entry->image->clusterSize;
        for (u32 i = 1; i < clusterCount; ++i)
        {
            file->cluster = file->entry->image->getNextCluster(file->entry->image, file->cluster);
            seekTo(file->entry->image, file->cluster);
            fread(bytes, sizeof(u8), readSize, file->entry->image->fp);
            bytes += readSize;
        }

        // 読み込むべきクラスタのうち、最後のクラスタからバイト列を読み込む
        file->cluster = file->entry->image->getNextCluster(file->entry->image, file->cluster);
        seekTo(file->entry->image, file->cluster);
        readSize = newPositionOffset % file->entry->image->clusterSize;
        fread(bytes, sizeof(u8), readSize, file->entry->image->fp);
    }

    file->position += size;

    if (file->position % file->entry->image->clusterSize == 0)
    {
        // 現在のクラスタの終端に達していたら、次のクラスタに移る
        file->cluster = file->entry->image->getNextCluster(file->entry->image, file->cluster);
    }

    return size;
}
#pragma endregion

#pragma region Main utilities
// ヘルプを表示する
void printHelp()
{
    printf("Available commands:\n");
    printf("  cd PATH\tChange current directory\n");
    printf("  ls [PATH]\tList child entries\n");
    printf("  tree [PATH]\tList descendant entries\n");
    printf("  info [PATH]\tShow entry information\n");
    printf("  cat [PATH]\tShow entry data\n");
    printf("  help\tShow this help\n");
    printf("  exit\tStop program\n");
}

/**
 * 基準となるエントリに相対的な、または絶対的なエントリを取得する
 * 成功したら0、それ以外の場合は0以外を返す
 */
Result getEntry(Entry **entryPointer, const Entry *baseEntry, const char *path)
{
    wchar_t *widePath;
    toWide(&widePath, path);
    Result result;

    if (startsWith(widePath, PATH_DELIMITER))
    {
        result = openEntry(entryPointer, baseEntry->image, widePath);
    }
    else
    {
        result = getDescendantEntry(entryPointer, baseEntry, widePath);
    }

    free(widePath);
    return result;
}

// 子エントリをすべて表示する
void printChildren(const Entry *parent)
{
    Entry **children;
    u16 count = getChildren(&children, parent);
    if (count < 0)
    {
        printf("Error: %d\n", count);
        return;
    }

    for (u16 i = 0; i < count; ++i)
    {
        Entry *child = children[i];
        char type = child->directory ? 'd' : 'f';
        printf("%c %ls\n", type, child->name);
        closeEntry(child);
    }

    free(children);
}

// 指定されたエントリ以下の階層構造を表示する
void printTree(const Entry *root)
{
    // エントリをコピーする
    Entry *entry;
    Result result = copyEntry(&entry, root);
    if (result)
    {
        printf("Error: %d\n", result);
        return;
    }

    // エントリのスタックのノード
    typedef struct __EntryNode
    {
        // エントリの根からの深さ
        u8 depth;

        // 末尾のエントリかどうか
        Boolean tail;

        // エントリ
        Entry *entry;

        // 次のノード
        struct __EntryNode *nextNode;
    } EntryNode;

    // スタックにエントリを格納していく
    EntryNode *entryStack = malloc(sizeof(EntryNode));
    entryStack->depth = 0;
    entryStack->entry = entry;
    entryStack->nextNode = NULL;

    while (entryStack != NULL)
    {
        // スタックからエントリをポップする
        EntryNode *node = entryStack;
        entryStack = node->nextNode;

        // インデントを下げる
        for (u8 i = 1; i < node->depth; ++i)
        {
            printf("    ");
        }

        // 親子関係を表す線を引く
        if (node->depth > 0)
        {
            printf(node->tail ? "└── " : "├── ");
        }

        // エントリの名前を表示する
        printf("%ls\n", node->entry->name);

        if (node->entry->directory)
        {
            Entry **children;
            u16 count = getChildren(&children, node->entry);
            if (count < 0)
            {
                printf("Error: %d\n", count);
            }
            else if (count > 0)
            {
                u8 nextDepth = node->depth + 1;

                // スタックにエントリをプッシュする
                EntryNode *newNode = malloc(sizeof(EntryNode));
                newNode->depth = nextDepth;
                newNode->tail = TRUE;
                newNode->entry = children[count - 1];
                newNode->nextNode = entryStack;
                entryStack = newNode;

                for (s16 i = count - 2; i >= 0; --i)
                {
                    Entry *child = children[i];
                    if (startsWith(child->name, L"."))
                    {
                        continue;
                    }

                    newNode = malloc(sizeof(EntryNode));
                    newNode->depth = nextDepth;
                    newNode->tail = FALSE;
                    newNode->entry = child;
                    newNode->nextNode = entryStack;
                    entryStack = newNode;
                }
            }

            free(children);
        }

        closeEntry(node->entry);
        free(node);
    }
}

// Datetimeを表示する
void printDatetime(Datetime *datetime)
{
    printf("%04d/%02d/%02d %02d:%02d:%02d.%04d\n",
           datetime->year,
           datetime->month,
           datetime->dayOfMonth,
           datetime->hour,
           datetime->minute,
           datetime->second,
           datetime->millisecond);
}

// エントリの情報を表示する
void printInfo(const Entry *entry)
{
    printf("Name: %ls\n", entry->name);
    printf("Attribute(s):");
    printf(entry->readonly ? " Readonly" : "");
    printf(entry->hidden ? " Hidden" : "");
    printf(entry->system ? " System" : "");
    printf(entry->volume ? " Volume" : "");
    printf(entry->directory ? " Directory" : "");
    printf(entry->file ? " File" : "");
    puts("");
    printf("Create: ");
    printDatetime(entry->createdAt);
    printf("Modify: ");
    printDatetime(entry->modifiedAt);
    printf("Access: ");
    printDatetime(entry->accessedAt);
    printf("Size: %dB\n", entry->size);
}

// ファイルの内容をすべて表示する
void printData(Entry *entry)
{
    if (!entry->file)
    {
        printf("Not file: %ls\n", entry->name);
        return;
    }

    File *file;
    Result result = openFile(&file, entry);
    if (result)
    {
        printf("Error: %d\n", result);
    }

    u8 bytes[201];
    while (TRUE)
    {
        u32 size = readFile(bytes, sizeof(bytes) - 1, file);
        if (size <= 0)
        {
            break;
        }

        bytes[size] = '\0';
        printf("%s", bytes);
    }

    puts("");

    closeFile(file);
}
#pragma endregion

// エントリのメタ情報を表示する
void printMeta(const Entry *entry)
{
    wchar_t basename[256];
    wchar_t extension[256];
    getBasenameAndExtension(entry->name, basename, extension);

    printf("name=%ls ", basename);
    printf("ext=%ls ", extension);
    printf("cTime=%d:%d:%d ", entry->createdAt->hour, entry->createdAt->minute, entry->createdAt->second);
    printf("cDate=%d/%d/%d ", entry->createdAt->year, entry->createdAt->month, entry->createdAt->dayOfMonth);
    printf("clusLow=%d ", entry->cluster & 0xfff);
    printf("size=%d\n", entry->size);
}

int main(int argc, char *argv[])
{
    char *imageFilename = "os23flp.img";
    wchar_t *targetFilename = L"HELLO.TXT";

    if (argc > 1)
    {
        imageFilename = argv[1];
    }
    if (argc > 2)
    {
        toWide(&targetFilename, argv[2]);
    }

    Image *image;
    Result result = openImage(&image, imageFilename);
    if (result)
    {
        return result;
    }

    Entry *entry;
    result = openEntry(&entry, image, targetFilename);
    if (result == 0)
    {
        printMeta(entry);
        printData(entry);
        closeEntry(entry);
    }

    if (argc > 2)
    {
        return result;
    }

    Entry *currentDirectory;
    result = openEntry(&currentDirectory, image, L"/");
    if (result)
    {
        return result;
    }

    printHelp();

    while (TRUE)
    {
        // プロンプトを表示する
        printf("> ");

        // コマンドの入力を受け付ける
        char line[256] = {0};
        fgets(line, sizeof(line), stdin);
        line[strcspn(line, "\n")] = '\0';

        // 入力をコマンドと引数に分割する
        char *command = strtok(line, " ");
        char *param = strtok(NULL, " ");
        if (param == NULL)
        {
            param = "";
        }

        // 引数が指すエントリを取得する
        Entry *paramEntry;
        result = getEntry(&paramEntry, currentDirectory, param);
        if (result)
        {
            printf("Error: %d\n", result);
            continue;
        }

        // コマンドごとに処理を行う
        if (strcmp(command, "cd") == 0)
        {
            Entry *temp = currentDirectory;
            currentDirectory = paramEntry;
            paramEntry = temp;
        }
        else if (strcmp(command, "ls") == 0)
        {
            printChildren(paramEntry);
        }
        else if (strcmp(command, "tree") == 0)
        {
            printTree(paramEntry);
        }
        else if (strcmp(command, "info") == 0)
        {
            printInfo(paramEntry);
        }
        else if (strcmp(command, "cat") == 0)
        {
            printData(paramEntry);
        }
        else if (strcmp(command, "help") == 0)
        {
            printHelp();
        }

        closeEntry(paramEntry);

        if (strcmp(command, "exit") == 0)
        {
            break;
        }
    }

    closeEntry(currentDirectory);

    closeImage(image);

    return result;
}
