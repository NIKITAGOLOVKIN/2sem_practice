#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char** environ;
#endif

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    NUMBER_OF_RUNS = 10,
    MAXIMUM_FILE_COUNT = 64,
    PATH_LENGTH = 256
};

static const char* archiverPath(void)
{
#ifdef _WIN32
    return "build\\huff.exe";
#else
    return "build/huff";
#endif
}

static int createDirectory(const char* path)
{
#ifdef _WIN32
    if (_mkdir(path) == 0 || errno == EEXIST)
        return 0;
#else
    if (mkdir(path, 0777) == 0 || errno == EEXIST)
        return 0;
#endif
    return -1;
}

static int fileSize(const char* path, uint64_t* size)
{
    struct stat information;

    if (stat(path, &information) != 0)
        return -1;
    *size = (uint64_t)information.st_size;
    return 0;
}

static int removeTree(const char* path)
{
#ifdef _WIN32
    char pattern[512];
    char childPath[512];
    WIN32_FIND_DATAA found;
    HANDLE search;

    snprintf(pattern, sizeof(pattern), "%s/*", path);
    search = FindFirstFileA(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
        return 0;

    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0)
            continue;
        snprintf(childPath, sizeof(childPath), "%s/%s", path, found.cFileName);
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (removeTree(childPath) != 0) {
                FindClose(search);
                return -1;
            }
        } else if (!DeleteFileA(childPath)) {
            FindClose(search);
            return -1;
        }
    } while (FindNextFileA(search, &found));
    FindClose(search);
    if (!RemoveDirectoryA(path))
        return -1;
    return 0;
#else
    DIR* directory = opendir(path);
    struct dirent* entry;

    if (directory == NULL)
        return errno == ENOENT ? 0 : -1;

    while ((entry = readdir(directory)) != NULL) {
        char childPath[512];
        struct stat information;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(childPath, sizeof(childPath), "%s/%s", path, entry->d_name);
        if (stat(childPath, &information) != 0) {
            closedir(directory);
            return -1;
        }
        if (S_ISDIR(information.st_mode)) {
            if (removeTree(childPath) != 0) {
                closedir(directory);
                return -1;
            }
        } else if (remove(childPath) != 0) {
            closedir(directory);
            return -1;
        }
    }
    closedir(directory);
    return rmdir(path) == 0 ? 0 : -1;
#endif
}

static int runArchiver(const char* command, const char* extraPath, double* milliseconds)
{
#ifdef _WIN32
    char commandLine[1024];
    SECURITY_ATTRIBUTES security;
    HANDLE sink;
    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION processInfo;
    LARGE_INTEGER frequency;
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    DWORD exitCode = 1;

    if (snprintf(commandLine, sizeof(commandLine), "%s %s %s %s", archiverPath(), command, "experiments/output/out.huf", extraPath) >= (int)sizeof(commandLine))
        return -1;

    memset(&security, 0, sizeof(security));
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    sink = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, NULL);
    if (sink == INVALID_HANDLE_VALUE)
        return -1;

    memset(&startupInfo, 0, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = sink;
    startupInfo.hStdOutput = sink;
    startupInfo.hStdError = sink;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    if (!CreateProcessA(NULL, commandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startupInfo, &processInfo)) {
        CloseHandle(sink);
        return -1;
    }
    CloseHandle(sink);
    WaitForSingleObject(processInfo.hProcess, INFINITE);
    QueryPerformanceCounter(&end);
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    if (exitCode != 0)
        return -1;
    *milliseconds = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)frequency.QuadPart;
    return 0;
#else
    char* arguments[] = { (char*)archiverPath(), (char*)command, (char*)"experiments/output/out.huf", (char*)extraPath, NULL };
    posix_spawn_file_actions_t actions;
    pid_t processId;
    int status;
    struct timespec start;
    struct timespec end;

    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (posix_spawn(&processId, archiverPath(), &actions, NULL, arguments, environ) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        return -1;
    }
    posix_spawn_file_actions_destroy(&actions);
    if (waitpid(processId, &status, 0) < 0)
        return -1;
    clock_gettime(CLOCK_MONOTONIC, &end);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return -1;
    *milliseconds = (double)(end.tv_sec - start.tv_sec) * 1000.0;
    *milliseconds += ((double)end.tv_nsec - (double)start.tv_nsec) / 1000000.0;
    return 0;
#endif
}

static double mean(const double* values, int count)
{
    double sum = 0;
    int i;

    for (i = 0; i < count; ++i)
        sum += values[i];
    return sum / (double)count;
}

static double deviation(const double* values, int count)
{
    double average = mean(values, count);
    double sum = 0;
    int i;

    for (i = 0; i < count; ++i) {
        double delta = values[i] - average;
        sum += delta * delta;
    }
    return sqrt(sum / (double)(count - 1));
}

static int joinPath(char* destination, size_t destinationSize, const char* directory, const char* name)
{
    size_t directoryLength = strlen(directory);
    size_t nameLength = strlen(name);

    if (directoryLength + 1 + nameLength + 1 > destinationSize)
        return -1;
    memcpy(destination, directory, directoryLength);
    destination[directoryLength] = '/';
    memcpy(destination + directoryLength + 1, name, nameLength + 1);
    return 0;
}

static int collectFiles(const char* directory, char paths[][PATH_LENGTH], int* count)
{
#ifdef _WIN32
    char pattern[512];
    char childPath[PATH_LENGTH];
    WIN32_FIND_DATAA found;
    HANDLE search;

    if (joinPath(pattern, sizeof(pattern), directory, "*") != 0)
        return -1;
    search = FindFirstFileA(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
        return -1;

    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0)
            continue;
        if (joinPath(childPath, sizeof(childPath), directory, found.cFileName) != 0) {
            FindClose(search);
            return -1;
        }
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (collectFiles(childPath, paths, count) != 0) {
                FindClose(search);
                return -1;
            }
        } else {
            if (*count >= MAXIMUM_FILE_COUNT) {
                FindClose(search);
                return -1;
            }
            snprintf(paths[*count], PATH_LENGTH, "%s", childPath);
            *count += 1;
        }
    } while (FindNextFileA(search, &found));
    FindClose(search);
    return 0;
#else
    DIR* folder = opendir(directory);
    struct dirent* entry;

    if (folder == NULL)
        return -1;

    while ((entry = readdir(folder)) != NULL) {
        char childPath[PATH_LENGTH];
        struct stat information;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (joinPath(childPath, sizeof(childPath), directory, entry->d_name) != 0) {
            closedir(folder);
            return -1;
        }
        if (stat(childPath, &information) != 0) {
            closedir(folder);
            return -1;
        }
        if (S_ISDIR(information.st_mode)) {
            if (collectFiles(childPath, paths, count) != 0) {
                closedir(folder);
                return -1;
            }
        } else {
            if (*count >= MAXIMUM_FILE_COUNT) {
                closedir(folder);
                return -1;
            }
            snprintf(paths[*count], PATH_LENGTH, "%s", childPath);
            *count += 1;
        }
    }
    closedir(folder);
    return 0;
#endif
}

static int comparePaths(const void* left, const void* right)
{
    return strcmp((const char*)left, (const char*)right);
}

static void formatName(const char* relativePath, char* name, size_t nameSize)
{
    const char* start = relativePath;
    const char* prefix = "experiments/experimentsFiles/";
    const char* slash;
    size_t length;

    if (strncmp(relativePath, prefix, strlen(prefix)) == 0)
        start = relativePath + strlen(prefix);
    slash = strchr(start, '/');
    if (slash == NULL)
        slash = start + strlen(start);
    length = (size_t)(slash - start);
    if (length >= nameSize)
        length = nameSize - 1;
    memcpy(name, start, length);
    name[length] = '\0';
}

static int removeArchive(void)
{
    if (remove("experiments/output/out.huf") == 0 || errno == ENOENT)
        return 0;
    return -1;
}

static int measureFile(FILE* results, const char* formatName, const char* relativePath)
{
    double compressMilliseconds[NUMBER_OF_RUNS];
    double decompressMilliseconds[NUMBER_OF_RUNS];
    uint64_t originalSize = 0;
    uint64_t archiveSize = 0;
    double compressionPercent;
    double compressMean;
    double compressDeviation;
    double decompressMean;
    double decompressDeviation;
    int run;

    if (fileSize(relativePath, &originalSize) != 0)
        return -1;

    for (run = 0; run < NUMBER_OF_RUNS; ++run) {
        if (removeArchive() != 0)
            return -1;
        if (runArchiver("c", relativePath, &compressMilliseconds[run]) != 0)
            return -1;
    }
    if (fileSize("experiments/output/out.huf", &archiveSize) != 0)
        return -1;

    for (run = 0; run < NUMBER_OF_RUNS; ++run) {
        if (removeTree("experiments/output/restored") != 0)
            return -1;
        if (runArchiver("x", "experiments/output/restored", &decompressMilliseconds[run]) != 0)
            return -1;
    }

    compressionPercent = 100.0 * (double)archiveSize / (double)originalSize;
    compressMean = mean(compressMilliseconds, NUMBER_OF_RUNS);
    compressDeviation = deviation(compressMilliseconds, NUMBER_OF_RUNS);
    decompressMean = mean(decompressMilliseconds, NUMBER_OF_RUNS);
    decompressDeviation = deviation(decompressMilliseconds, NUMBER_OF_RUNS);

    printf("%s\t%s\t%.2f\t%.2f +/- %.2f\t%.2f +/- %.2f\n",
        formatName,
        relativePath,
        compressionPercent,
        compressMean,
        compressDeviation,
        decompressMean,
        decompressDeviation);
    fflush(stdout);
    if (results != NULL) {
        fprintf(results, "%s,%s,%llu,%llu,%.2f,%.2f,%.2f,%.2f,%.2f\n",
            formatName,
            relativePath,
            (unsigned long long)originalSize,
            (unsigned long long)archiveSize,
            compressionPercent,
            compressMean,
            compressDeviation,
            decompressMean,
            decompressDeviation);
        fflush(results);
    }
    return 0;
}

int main(void)
{
    FILE* results;
    uint64_t archiverSize = 0;

    if (fileSize(archiverPath(), &archiverSize) != 0) {
        fprintf(stderr, "Не найден %s. Сначала соберите архиватор и запустите программу из корня репозитория.\n", archiverPath());
        return 1;
    }
    if (createDirectory("experiments/output") != 0) {
        fprintf(stderr, "Не удалось создать каталог %s\n", "experiments/output");
        return 1;
    }

    results = fopen("experiments/output/experimentResults.csv", "w");
    if (results != NULL) {
        fprintf(results, "format,path,original,archive,percent,compressMs,compressSigma,decompressMs,decompressSigma\n");
    }

    printf("format\tpath\tpercent\tcompress ms\tdecompress ms\n");
    {
        char paths[MAXIMUM_FILE_COUNT][PATH_LENGTH];
        int fileCount = 0;
        int fileIndex;

        if (collectFiles("experiments/experimentsFiles", paths, &fileCount) != 0 || fileCount == 0) {
            fprintf(stderr, "Не удалось прочитать файлы из %s\n", "experiments/experimentsFiles");
            if (results != NULL)
                fclose(results);
            return 1;
        }
        qsort(paths, (size_t)fileCount, PATH_LENGTH, comparePaths);
        for (fileIndex = 0; fileIndex < fileCount; ++fileIndex) {
            char name[64];

            formatName(paths[fileIndex], name, sizeof(name));
            if (measureFile(results, name, paths[fileIndex]) != 0) {
                fprintf(stderr, "Не удалось измерить %s\n", paths[fileIndex]);
                if (results != NULL)
                    fclose(results);
                return 1;
            }
        }
    }

    removeTree("experiments/output/restored");
    removeArchive();
    if (results != NULL)
        fclose(results);
    printf("Результаты записаны в experiments/output/experimentResults.csv\n");
    return 0;
}
