// FileTiming.cpp --- Command-Line File Time Utility
// SETTING FILE TIME AND DOING STATISTICS FOR FILES AND FOLDERS.
// Copyright (C) 2014 Katayama Hirofumi MZ. All Rights Reserved.

#include <windows.h>
#include <vector>
#include <map>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <tchar.h>
using namespace std;

#ifdef UNICODE
    typedef std::wstring tstring;
#else
    typedef std::string tstring;
#endif

#define ISDOTS(psz) ((psz)[0] == '.' && ((psz)[1] == '\0' || (psz)[1] == '.' && (psz)[2] == '\0'))

BOOL g_bDoForHidden = FALSE;
FILETIME g_ftCreate, g_ftAccess, g_ftWrite;

INT g_nTotal = 0;
INT g_nSuccesses = 0;
std::vector<FILETIME> g_vecftCreate;
std::vector<FILETIME> g_vecftAccess;
std::vector<FILETIME> g_vecftWrite;

enum {
    DO_STAT_DATE,
    DO_STAT_YEAR,
    DO_STAT_MONTH,
    DO_STAT_MONTHDAY,
    DO_STAT_WEEKDAY,
    DO_STAT_TIME,
    DO_STAT_ALL
} g_stat_type = DO_STAT_DATE;

std::map<tstring, ULONGLONG> g_mapStat;

BOOL g_bSetTimeAtRandom = FALSE;
FILETIME g_ftRandomMin, g_ftRandomMax;

VOID GenerateRandomFileTimeInRange(FILETIME *pft) {
    ULARGE_INTEGER uli0, uli1, uli2;
    assert(pft);

    uli1.LowPart = g_ftRandomMin.dwLowDateTime;
    uli1.HighPart = g_ftRandomMin.dwHighDateTime;
    uli2.LowPart = g_ftRandomMax.dwLowDateTime;
    uli2.HighPart = g_ftRandomMax.dwHighDateTime;
    assert(uli1.QuadPart <= uli2.QuadPart);

    ULONGLONG ullDiff = uli2.QuadPart - uli1.QuadPart;
    DWORD r30 = RAND_MAX * rand() + rand();
    DWORD s30 = RAND_MAX * rand() + rand();
    DWORD t4 = rand() & 0xF;
    ULONGLONG ullAdd = ((ULONGLONG)r30 << 34) | (s30 << 4) | t4;
    ullAdd %= ullDiff;

    uli0.QuadPart = uli1.QuadPart;
    uli0.QuadPart += ullAdd;
    pft->dwLowDateTime = uli0.LowPart;
    pft->dwHighDateTime = uli0.HighPart;
}

BOOL DoSetFileTimeOfFile(LPCTSTR pszFile) {
    HANDLE hFile;
    BOOL success = FALSE;

    hFile = CreateFile(pszFile, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    );
    if (hFile != INVALID_HANDLE_VALUE) {
        if (g_bSetTimeAtRandom) {
            FILETIME ft, ft2;
            GenerateRandomFileTimeInRange(&ft);
            LocalFileTimeToFileTime(&ft, &ft2);
            g_ftCreate = ft2;
            GenerateRandomFileTimeInRange(&ft);
            LocalFileTimeToFileTime(&ft, &ft2);
            g_ftAccess = ft2;
            GenerateRandomFileTimeInRange(&ft);
            LocalFileTimeToFileTime(&ft, &ft2);
            g_ftWrite = ft2;
        }
        if (SetFileTime(hFile, &g_ftCreate, &g_ftAccess, &g_ftWrite)) {
            g_nSuccesses++;
            success = TRUE;
        }
        CloseHandle(hFile);
    }
    return success;
}

BOOL DoSetFileTimesOfDir(LPCTSTR pszDir) {
    TCHAR szDirOld[MAX_PATH];
    HANDLE hFind;
    WIN32_FIND_DATA find;

    GetCurrentDirectory(MAX_PATH, szDirOld);
    if (!SetCurrentDirectory(pszDir))
        return FALSE;

    hFind = FindFirstFile(TEXT("*"), &find);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!ISDOTS(find.cFileName)) {
                if (!g_bDoForHidden) {
                    if (find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
                        continue;
                    }
                }
                if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    TCHAR sz[MAX_PATH];
                    if (!DoSetFileTimesOfDir(find.cFileName)) {
                        GetFullPathName(find.cFileName, MAX_PATH, sz, NULL);
                        _ftprintf(stderr, TEXT("ERROR: Failure on Folder '%s'\n"), sz);
                    }
                } else {
                    TCHAR sz[MAX_PATH];
                    if (!DoSetFileTimeOfFile(find.cFileName)) {
                        GetFullPathName(find.cFileName, MAX_PATH, sz, NULL);
                        _ftprintf(stderr, TEXT("ERROR: Failure on File '%s'\n"), sz);
                    }
                }
            }
        } while(FindNextFile(hFind, &find));
        FindClose(hFind);
    }
    SetCurrentDirectory(szDirOld);

    return TRUE;
}

BOOL DoSetFileTimesOfDir2(LPCTSTR pszDir) {
    TCHAR szDir[MAX_PATH];
    GetFullPathName(pszDir, MAX_PATH, szDir, NULL);
    return DoSetFileTimesOfDir(szDir);
}

BOOL DoAddStat(BOOL success) {
    if (success) {
        g_vecftCreate.push_back(g_ftCreate);
        g_vecftAccess.push_back(g_ftAccess);
        g_vecftWrite.push_back(g_ftWrite);
        g_nSuccesses++;
    }
    return TRUE;
}

BOOL DoStatFileTimeOfFile(LPCTSTR pszFile) {
    HANDLE hFile;
    BOOL success = FALSE;

    hFile = CreateFile(pszFile, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    );
    if (hFile != INVALID_HANDLE_VALUE) {
        success = GetFileTime(hFile, &g_ftCreate, &g_ftAccess, &g_ftWrite);
        DoAddStat(success);
        CloseHandle(hFile);
    }
    return success;
}

BOOL DoStatFileTimesOfDir(LPCTSTR pszDir) {
    TCHAR szDirOld[MAX_PATH];
    HANDLE hFind;
    WIN32_FIND_DATA find;

    GetCurrentDirectory(MAX_PATH, szDirOld);
    if (!SetCurrentDirectory(pszDir))
        return FALSE;

    hFind = FindFirstFile(TEXT("*"), &find);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!ISDOTS(find.cFileName)) {
                if (!g_bDoForHidden) {
                    if (find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
                        continue;
                    }
                }
                if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (!DoStatFileTimesOfDir(find.cFileName)) {
                        TCHAR sz[MAX_PATH];
                        GetFullPathName(find.cFileName, MAX_PATH, sz, NULL);
                        _ftprintf(stderr, TEXT("ERROR: Failure on Folder '%s'\n"), sz);
                    }
                } else {
                    if (!DoStatFileTimeOfFile(find.cFileName)) {
                        TCHAR sz[MAX_PATH];
                        GetFullPathName(find.cFileName, MAX_PATH, sz, NULL);
                        _ftprintf(stderr, TEXT("ERROR: Failure on File '%s'\n"), sz);
                    }
                }
            }
        } while(FindNextFile(hFind, &find));
        FindClose(hFind);
    }
    SetCurrentDirectory(szDirOld);

    return TRUE;
}

BOOL DoStatFileTimesOfDir2(LPCTSTR pszDir) {
    TCHAR szDir[MAX_PATH];
    GetFullPathName(pszDir, MAX_PATH, szDir, NULL);
    return DoStatFileTimesOfDir(szDir);
}

BOOL DoPrintStat(VOID) {
    std::map<tstring, ULONGLONG>::iterator it, end;
    end = g_mapStat.end();
    for (it = g_mapStat.begin(); it != end; ++it) {
        _tprintf(TEXT("%s: %I64u\n"), it->first.data(), it->second);
    }
    return TRUE;
}

BOOL DoPrintStatByYear(const std::vector<FILETIME>& vecft) {
    FILETIME ftLocal;
    SYSTEMTIME st;
    ULARGE_INTEGER uli;
    TCHAR sz[64];

    g_mapStat.clear();
    size_t siz = vecft.size();
    for (size_t i = 0; i < siz; ++i) {
        FileTimeToLocalFileTime(&vecft[i], &ftLocal);
        FileTimeToSystemTime(&ftLocal, &st);
        wsprintf(sz, TEXT("Year %04u"), st.wYear);
        g_mapStat[sz]++;
    }
    return DoPrintStat();
}

BOOL DoPrintStatByMonth(const std::vector<FILETIME>& vecft) {
    FILETIME ftLocal;
    SYSTEMTIME st;
    ULARGE_INTEGER uli;
    TCHAR sz[64];

    g_mapStat.clear();
    size_t siz = vecft.size();
    for (size_t i = 0; i < siz; ++i) {
        FileTimeToLocalFileTime(&vecft[i], &ftLocal);
        FileTimeToSystemTime(&ftLocal, &st);
        wsprintf(sz, TEXT("Month %02u"), st.wMonth);
        g_mapStat[sz]++;
    }
    return DoPrintStat();
}

BOOL DoPrintStatByMonthDay(const std::vector<FILETIME>& vecft) {
    FILETIME ftLocal;
    SYSTEMTIME st;
    ULARGE_INTEGER uli;
    TCHAR sz[64];

    g_mapStat.clear();
    size_t siz = vecft.size();
    for (size_t i = 0; i < siz; ++i) {
        FileTimeToLocalFileTime(&vecft[i], &ftLocal);
        FileTimeToSystemTime(&ftLocal, &st);
        wsprintf(sz, TEXT("Day %02u"), st.wDay);
        g_mapStat[sz]++;
    }
    return DoPrintStat();
}

BOOL DoPrintStatByDate(const std::vector<FILETIME>& vecft) {
    FILETIME ftLocal;
    SYSTEMTIME st;
    ULARGE_INTEGER uli;
    TCHAR sz[64];

    g_mapStat.clear();
    size_t siz = vecft.size();
    for (size_t i = 0; i < siz; ++i) {
        FileTimeToLocalFileTime(&vecft[i], &ftLocal);
        FileTimeToSystemTime(&ftLocal, &st);
        wsprintf(sz, TEXT("Date %04u.%02u.%02u"), st.wYear, st.wMonth, st.wDay);
        g_mapStat[sz]++;
    }
    return DoPrintStat();
}

BOOL DoPrintStatByWeekDay(const std::vector<FILETIME>& vecft) {
    FILETIME ftLocal;
    SYSTEMTIME st;

    static const LPCTSTR s_aWeekDay[] = {
        TEXT("#1 Sun"),
        TEXT("#2 Mon"),
        TEXT("#3 Tue"),
        TEXT("#4 Wed"),
        TEXT("#5 Thu"),
        TEXT("#6 Fri"),
        TEXT("#7 Sat")
    };

    g_mapStat.clear();
    size_t siz = vecft.size();
    for (size_t i = 0; i < siz; ++i) {
        FileTimeToLocalFileTime(&vecft[i], &ftLocal);
        FileTimeToSystemTime(&ftLocal, &st);
        g_mapStat[s_aWeekDay[st.wDayOfWeek]]++;
    }
    return DoPrintStat();
}

BOOL DoPrintStatByTime(const std::vector<FILETIME>& vecft) {
    FILETIME ftLocal;
    SYSTEMTIME st;
    ULARGE_INTEGER uli;
    TCHAR sz[64];

    g_mapStat.clear();
    size_t siz = vecft.size();
    for (size_t i = 0; i < siz; ++i) {
        FileTimeToLocalFileTime(&vecft[i], &ftLocal);
        FileTimeToSystemTime(&ftLocal, &st);
        wsprintf(sz, TEXT("Hour %02u"), st.wHour);
        g_mapStat[sz]++;
    }
    return DoPrintStat();
}

BOOL DoPrintFileTimeStatOfFile(LPCTSTR pszFile) {
    if (DoStatFileTimeOfFile(pszFile)) {
        printf("\n[ CREATION TIMES ]\n");
        DoPrintStatByYear(g_vecftCreate);
        printf("\n[ ACCESS TIMES ]\n");
        DoPrintStatByYear(g_vecftAccess);
        printf("\n[ LAST MODIFIED TIMES ]\n");
        DoPrintStatByYear(g_vecftWrite);
    } else {
        _tprintf(TEXT("ERROR: Failure on File '%s'\n"), pszFile);
    }
    return TRUE;
}

BOOL DoPrintFileTimesStatOfDir(LPCTSTR pszDir) {
    if (DoStatFileTimesOfDir2(pszDir)) {
        switch (g_stat_type) {
        case DO_STAT_YEAR:
            printf("\n[ CREATION TIMES ]\n");
            DoPrintStatByYear(g_vecftCreate);
            printf("\n[ ACCESS TIMES ]\n");
            DoPrintStatByYear(g_vecftAccess);
            printf("\n[ LAST MODIFIED TIMES ]\n");
            DoPrintStatByYear(g_vecftWrite);
            break;

        case DO_STAT_MONTH:
            printf("\n[ CREATION TIMES ]\n");
            DoPrintStatByMonth(g_vecftCreate);
            printf("\n[ ACCESS TIMES ]\n");
            DoPrintStatByMonth(g_vecftAccess);
            printf("\n[ LAST MODIFIED TIMES ]\n");
            DoPrintStatByMonth(g_vecftWrite);
            break;

        case DO_STAT_DATE:
            printf("\n[ CREATION TIMES ]\n");
            DoPrintStatByDate(g_vecftCreate);
            printf("\n[ ACCESS TIMES ]\n");
            DoPrintStatByDate(g_vecftAccess);
            printf("\n[ LAST MODIFIED TIMES ]\n");
            DoPrintStatByDate(g_vecftWrite);
            break;

        case DO_STAT_MONTHDAY:
            printf("\n[ CREATION TIMES ]\n");
            DoPrintStatByMonthDay(g_vecftCreate);
            printf("\n[ ACCESS TIMES ]\n");
            DoPrintStatByMonthDay(g_vecftAccess);
            printf("\n[ LAST MODIFIED TIMES ]\n");
            DoPrintStatByMonthDay(g_vecftWrite);
            break;

        case DO_STAT_WEEKDAY:
            printf("\n[ CREATION TIMES ]\n");
            DoPrintStatByWeekDay(g_vecftCreate);
            printf("\n[ ACCESS TIMES ]\n");
            DoPrintStatByWeekDay(g_vecftAccess);
            printf("\n[ LAST MODIFIED TIMES ]\n");
            DoPrintStatByWeekDay(g_vecftWrite);
            break;

        case DO_STAT_TIME:
            printf("\n[ CREATION TIMES ]\n");
            DoPrintStatByTime(g_vecftCreate);
            printf("\n[ ACCESS TIMES ]\n");
            DoPrintStatByTime(g_vecftAccess);
            printf("\n[ LAST MODIFIED TIMES ]\n");
            DoPrintStatByTime(g_vecftWrite);
            break;

        case DO_STAT_ALL:
            printf("\n[ CREATION TIMES ]\n");
            DoPrintStatByDate(g_vecftCreate);
            DoPrintStatByYear(g_vecftCreate);
            DoPrintStatByMonth(g_vecftCreate);
            DoPrintStatByMonthDay(g_vecftCreate);
            DoPrintStatByWeekDay(g_vecftCreate);
            DoPrintStatByTime(g_vecftCreate);

            printf("\n[ ACCESS TIMES ]\n");
            DoPrintStatByDate(g_vecftAccess);
            DoPrintStatByYear(g_vecftAccess);
            DoPrintStatByMonth(g_vecftAccess);
            DoPrintStatByMonthDay(g_vecftAccess);
            DoPrintStatByWeekDay(g_vecftAccess);
            DoPrintStatByTime(g_vecftAccess);

            printf("\n[ LAST MODIFIED TIMES ]\n");
            DoPrintStatByDate(g_vecftWrite);
            DoPrintStatByYear(g_vecftWrite);
            DoPrintStatByMonth(g_vecftWrite);
            DoPrintStatByMonthDay(g_vecftWrite);
            DoPrintStatByWeekDay(g_vecftWrite);
            DoPrintStatByTime(g_vecftWrite);
            break;
        }
    } else {
        _tprintf(TEXT("ERROR: Failure on File '%s'\n"), pszDir);
    }
    return TRUE;
}

BOOL DoPrintFileTimesStatOfDir2(LPCTSTR pszDir) {
    TCHAR szDir[MAX_PATH];
    GetFullPathName(pszDir, MAX_PATH, szDir, NULL);
    return DoPrintFileTimesStatOfDir(szDir);
}

BOOL DoFileCountOfDir(LPCTSTR pszDir) {
    TCHAR szDirOld[MAX_PATH];
    HANDLE hFind;
    WIN32_FIND_DATA find;

    GetCurrentDirectory(MAX_PATH, szDirOld);
    if (!SetCurrentDirectory(pszDir))
        return FALSE;

    hFind = FindFirstFile(TEXT("*"), &find);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!ISDOTS(find.cFileName)) {
                if (!g_bDoForHidden) {
                    if (find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
                        continue;
                    }
                }
                if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    DoFileCountOfDir(find.cFileName);
                } else {
                    g_nTotal++;
                }
            }
        } while(FindNextFile(hFind, &find));
        FindClose(hFind);
    }
    SetCurrentDirectory(szDirOld);

    return TRUE;
}

BOOL DoFileCountOfDir2(LPCTSTR pszDir) {
    TCHAR szDir[MAX_PATH];
    GetFullPathName(pszDir, MAX_PATH, szDir, NULL);
    return DoFileCountOfDir(szDir);
}

BOOL ParseDateTime(LPFILETIME pftLocalTime, LPTSTR pszTime) {
    SYSTEMTIME st;
    FILETIME ft;
    TCHAR sz[32];

    ZeroMemory(&st, sizeof(st));
    if (lstrlen(pszTime) == 8 || lstrlen(pszTime) == 14) {
        sz[0] = pszTime[0];
        sz[1] = pszTime[1];
        sz[2] = pszTime[2];
        sz[3] = pszTime[3];
        sz[4] = 0;
        st.wYear = _ttoi(sz);
        sz[0] = pszTime[4];
        sz[1] = pszTime[5];
        sz[2] = 0;
        st.wMonth = _ttoi(sz);
        sz[0] = pszTime[6];
        sz[1] = pszTime[7];
        sz[2] = 0;
        st.wDay = _ttoi(sz);
        if (pszTime[8]) {
            sz[0] = pszTime[8];
            sz[1] = pszTime[9];
            sz[2] = 0;
            st.wHour = _ttoi(sz);
            sz[0] = pszTime[10];
            sz[1] = pszTime[11];
            sz[2] = 0;
            st.wMinute = _ttoi(sz);
            sz[0] = pszTime[12];
            sz[1] = pszTime[13];
            sz[2] = 0;
            st.wSecond = _ttoi(sz);
        }
        if (SystemTimeToFileTime(&st, pftLocalTime)) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL ParseDateTimeRange(LPFILETIME pftLocalTime1, LPFILETIME pftLocalTime2, LPTSTR pszTime) {
    tstring t = pszTime;
    size_t i = t.find(TEXT('-'));
    if (i == tstring::npos) {
        return FALSE;
    }
    pszTime[i++] = 0;

    if (ParseDateTime(pftLocalTime1, pszTime)) {
        if (pszTime[i] == 0) {
            GetSystemTimeAsFileTime(pftLocalTime2);
        } else if (!ParseDateTime(pftLocalTime2, pszTime + i)) {
            return FALSE;
        }

        ULARGE_INTEGER uli1, uli2;
        uli1.LowPart = pftLocalTime1->dwLowDateTime;
        uli1.HighPart = pftLocalTime1->dwHighDateTime;
        uli2.LowPart = pftLocalTime2->dwLowDateTime;
        uli2.HighPart = pftLocalTime2->dwHighDateTime;
        return uli1.QuadPart <= uli2.QuadPart;
    }
    return FALSE;
}

BOOL AddInputsWildCard(std::vector<tstring>& inputs, LPCTSTR pszWildCard)
{
    HANDLE hFind;
    WIN32_FIND_DATA find;
    TCHAR szOldCurDir[MAX_PATH], szDir[MAX_PATH];
    BOOL success = FALSE;

    lstrcpyn(szDir, pszWildCard, MAX_PATH);
    LPTSTR pch = _tcsrchr(szDir, TEXT('\\'));
    if (pch == NULL) {
        hFind = FindFirstFile(pszWildCard, &find);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!ISDOTS(find.cFileName)) {
                    inputs.push_back(find.cFileName);
                    success = TRUE;
                }
            } while (FindNextFile(hFind, &find));
            FindClose(hFind);
        }
    } else {
        *pch = 0;
        GetCurrentDirectory(MAX_PATH, szOldCurDir);
        if (SetCurrentDirectory(szDir)) {
            hFind = FindFirstFile(pszWildCard, &find);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (!ISDOTS(find.cFileName)) {
                        inputs.push_back(find.cFileName);
                        success = TRUE;
                    }
                } while (FindNextFile(hFind, &find));
                FindClose(hFind);
            }
            SetCurrentDirectory(szOldCurDir);
        }
    }

    return success;
}

extern "C"
int _tmain(int argc, TCHAR *argv[]) {
    std::vector<tstring> inputs;
    BOOL bSetTime = FALSE;
    FILETIME ftLocal;

    srand(GetTickCount() ^ GetCurrentProcessId());

    if (argc >= 2 &&
        (lstrcmpi(argv[1], TEXT("--help")) == 0 ||
         lstrcmpi(argv[1], TEXT("/?")) == 0 ||
         lstrcmpi(argv[1], TEXT("-?")) == 0))
    {
        printf("Usage: FileTiming [options] [directory]\n");
        printf("Options:\n");
        printf("-E                    Do stat for dates.\n");
        printf("-Y                    Do stat for years.\n");
        printf("-M                    Do stat for months.\n");
        printf("-D                    Do stat for days of month.\n");
        printf("-W                    Do stat for days of week.\n");
        printf("-T                    Do stat for times of day.\n");
        printf("-A                    Do stat for all information.\n");
        printf("-H                    Do also for hidden files/directories.\n");
        printf("-N                    Do set file time to now.\n");
        printf("-S YYYYMMDD           Do set file date (YYYYMMDD).\n");
        printf("-S YYYYMMDDHHmmss     Do set file date (YYYYMMDD) and time (HHmmss).\n");
        printf("-R YYYYMMDD-          Do set file date and time at random in the range.\n");
        printf("-R YYYYMMDD-YYYYMMDD  Do set file date and time at random in the range.\n");
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        if (lstrcmpi(argv[i], TEXT("-E")) == 0) {
            g_stat_type = DO_STAT_DATE;
        } else if (lstrcmpi(argv[i], TEXT("-Y")) == 0) {
            g_stat_type = DO_STAT_YEAR;
        } else if (lstrcmpi(argv[i], TEXT("-M")) == 0) {
            g_stat_type = DO_STAT_MONTH;
        } else if (lstrcmpi(argv[i], TEXT("-D")) == 0) {
            g_stat_type = DO_STAT_MONTHDAY;
        } else if (lstrcmpi(argv[i], TEXT("-W")) == 0) {
            g_stat_type = DO_STAT_WEEKDAY;
        } else if (lstrcmpi(argv[i], TEXT("-T")) == 0) {
            g_stat_type = DO_STAT_TIME;
        } else if (lstrcmpi(argv[i], TEXT("-A")) == 0) {
            g_stat_type = DO_STAT_ALL;
        } else if (lstrcmpi(argv[i], TEXT("-H")) == 0) {
            g_bDoForHidden = TRUE;
        } else if (lstrcmpi(argv[i], TEXT("-N")) == 0) {
            bSetTime = TRUE;
            FILETIME ftSystem;
            GetSystemTimeAsFileTime(&ftSystem);
            FileTimeToLocalFileTime(&ftSystem, &ftLocal);
        } else if (lstrcmpi(argv[i], TEXT("-S")) == 0) {
            bSetTime = TRUE;
            i++;
            if (!ParseDateTime(&ftLocal, argv[i])) {
                fprintf(stderr, "ERROR: Invalid time format for -S\n");
                return 1;
            }
        } else if (lstrcmpi(argv[i], TEXT("-R")) == 0) {
            g_bSetTimeAtRandom = TRUE;
            i++;
            if (!ParseDateTimeRange(&g_ftRandomMin, &g_ftRandomMax, argv[i])) {
                fprintf(stderr, "ERROR: Invalid time format for -R\n");
                return 1;
            }
        } else {
            if (_tcschr(argv[i], TEXT('?')) == NULL &&
                _tcschr(argv[i], TEXT('*')) == NULL)
            {
                inputs.push_back(argv[i]);
            } else {
                AddInputsWildCard(inputs, argv[i]);
            }
        }
    }

    if (inputs.empty()) {
        inputs.push_back(TEXT("."));
    }

    size_t siz = inputs.size();

    if (bSetTime || g_bSetTimeAtRandom) {
        printf("[[ SETTING FILETIME ]]\n");
    } else {
        printf("[[ STATISTICS ]]\n");
    }

    g_nTotal = 0;
    for (size_t i = 0; i < siz; ++i) {
        DWORD attrs = GetFileAttributes(inputs[i].data());
        if (attrs == 0xFFFFFFFF) {
            _ftprintf(stderr, TEXT("ERROR: Not found file or directory: '%s'\n"), inputs[i].data());
            return 2;
        }
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            DoFileCountOfDir2(inputs[i].data());
        } else {
            g_nTotal++;
        }
    }

    for (size_t i = 0; i < siz; ++i) {
        DWORD attrs = GetFileAttributes(inputs[i].data());
        if (attrs == 0xFFFFFFFF) {
            _ftprintf(stderr, TEXT("ERROR: Not found file or directory: '%s'\n"), inputs[i].data());
            return 2;
        }
        if (bSetTime || g_bSetTimeAtRandom) {
            FILETIME ftSystem;
            LocalFileTimeToFileTime(&ftLocal, &ftSystem);
            g_ftCreate = g_ftAccess = g_ftWrite = ftSystem;
            if (g_bSetTimeAtRandom) {
                SYSTEMTIME st1, st2;
                FileTimeToSystemTime(&g_ftRandomMin, &st1);
                FileTimeToSystemTime(&g_ftRandomMax, &st2);
                printf("\nSetting %04u.%02u.%02u %02u:%02u:%02u - %04u.%02u.%02u %02u:%02u:%02u...\n",
                    st1.wYear, st1.wMonth, st1.wDay,
                    st1.wHour, st1.wMinute, st1.wSecond,
                    st2.wYear, st2.wMonth, st2.wDay,
                    st2.wHour, st2.wMinute, st2.wSecond
                );
            } else {
                SYSTEMTIME st;
                FileTimeToSystemTime(&ftLocal, &st);
                printf("\nSetting %04u.%02u.%02u %02u:%02u:%02u...\n",
                    st.wYear, st.wMonth, st.wDay,
                    st.wHour, st.wMinute, st.wSecond
                );
            }
            if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                _tprintf(TEXT("DIRECTORY PATH: %s\n"), inputs[i].data());
                DoSetFileTimesOfDir2(inputs[i].data());
            } else {
                _tprintf(TEXT("FILE PATH: %s\n"), inputs[i].data());
                if (!DoSetFileTimeOfFile(inputs[i].data())) {
                    TCHAR sz[MAX_PATH];
                    GetFullPathName(inputs[i].data(), MAX_PATH, sz, NULL);
                    _ftprintf(stderr, TEXT("ERROR: Failure on File '%s'\n"), sz);
                }
            }
        } else {
            if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                _tprintf(TEXT("DIRECTORY PATH: %s\n"), inputs[i].data());
                DoPrintFileTimesStatOfDir2(inputs[i].data());
            } else {
                _tprintf(TEXT("FILE PATH: %s\n"), inputs[i].data());
                DoPrintFileTimeStatOfFile(inputs[i].data());
            }
        }
    }

    printf("\n%d Successes, %d Failures", g_nSuccesses, g_nTotal - g_nSuccesses);
    return 0;
}

