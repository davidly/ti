#define UNICODE

#include <stdio.h>
#include <windows.h>
#include <psapi.h>

static WCHAR g_awcApp[ MAX_PATH + 1 ] = {0};
static WCHAR g_awcProcess[ 32768 ] = {0};

class CPerfTime
{
    private:
        LARGE_INTEGER liLastCall;
        LARGE_INTEGER liFrequency;
        NUMBERFMT NumberFormat;
        WCHAR awcRender[ 100 ];

    public:
        CPerfTime()
        {
            ZeroMemory( &NumberFormat, sizeof NumberFormat );
            NumberFormat.NumDigits = 0;
            NumberFormat.Grouping = 3;
            NumberFormat.lpDecimalSep = L".";
            NumberFormat.lpThousandSep = L",";

            Baseline();
            QueryPerformanceFrequency( &liFrequency );
        } //CPerfTime

        void Baseline()
        {
            QueryPerformanceCounter( &liLastCall );
        } //Baseline
    
        int RenderLL( LONGLONG ll, WCHAR * pwcBuf, ULONG cwcBuf )
        {
            WCHAR awc[100];
            swprintf( awc, L"%I64u", ll );

            if ( 0 != cwcBuf )
                *pwcBuf = 0;

            return GetNumberFormat( LOCALE_USER_DEFAULT, 0, awc, &NumberFormat, pwcBuf, cwcBuf );
        } //RenderLL

        WCHAR * RenderLL( LONGLONG ll )
        {
            WCHAR awc[100];
            swprintf( awc, L"%I64u", ll );

            awcRender[0] = 0;

            GetNumberFormat( LOCALE_USER_DEFAULT, 0, awc, &NumberFormat, awcRender, _countof( awcRender ) );
            return awcRender;
        } //RenderLL

        void CumulateSince( LONGLONG & running )
        {
            LARGE_INTEGER liNow;
            QueryPerformanceCounter( &liNow );
            LONGLONG since = liNow.QuadPart - liLastCall.QuadPart;
            liLastCall = liNow;

            InterlockedExchangeAdd64( &running, since );
        } //CumulateSince
    
        LONGLONG DurationToMS( LONGLONG duration )
        {
            duration *= 1000000;
            return ( duration / liFrequency.QuadPart ) / 1000;
        }

        WCHAR * RenderDurationInMS( LONGLONG duration )
        {
            LONGLONG x = DurationToMS( duration );
            RenderLL( x, awcRender, _countof( awcRender ) );
            return awcRender;
        } //RenderDurationIsMS
}; //CPerfTime

void ShowProcessDetails( HANDLE hProc, LONGLONG elapsed, CPerfTime & perfTime, bool fOneLine )
{
    WCHAR awcRender[ MAX_PATH * 2 ];
    const int cwcRender = _countof( awcRender );

    wprintf( L"\n" );

    // QueryFullProcessImageNameW and GetModeuleFileNameEx fail with error 31 general failure if the process has already exited
    // If so, fall back to GetProcessImageFileNameW, which shows disk device names, not drive letters

    if ( !fOneLine )
    {
        if ( 0 != g_awcApp[0] )
            wprintf( L"process stats for %ws\n", g_awcApp );
        else if ( 0 != GetProcessImageFileNameW( hProc, awcRender, cwcRender ) )
            wprintf( L"process stats for: %ws\n", awcRender );

        PROCESS_MEMORY_COUNTERS_EX pmc;
        pmc.cb = sizeof pmc;
        
        if ( GetProcessMemoryInfo( hProc, (PPROCESS_MEMORY_COUNTERS) &pmc, sizeof PROCESS_MEMORY_COUNTERS_EX ) )
        {
            wprintf( L"peak working set:  %15ws\n", perfTime.RenderLL( pmc.PeakWorkingSetSize ) );
            wprintf( L"final working set: %15ws\n", perfTime.RenderLL( pmc.WorkingSetSize ) );
        }
    }

    SYSTEM_INFO si;
    GetSystemInfo( &si );
    int cpuCount = si.dwNumberOfProcessors;

    FILETIME creationFT, exitFT, kernelFT, userFT;
    if ( GetProcessTimes( hProc, &creationFT, &exitFT, &kernelFT, &userFT ) )
    {
        ULARGE_INTEGER ullK, ullU;
        ullK.HighPart = kernelFT.dwHighDateTime;
        ullK.LowPart = kernelFT.dwLowDateTime;
    
        ullU.HighPart = userFT.dwHighDateTime;
        ullU.LowPart = userFT.dwLowDateTime;

        double efficiency = (double) perfTime.DurationToMS( ullU.QuadPart + ullK.QuadPart ) /
                            ( (double) perfTime.DurationToMS( elapsed ) * (double) cpuCount );
        double coresUsed = efficiency * (double) cpuCount / 100.0;

        if ( fOneLine )
        {
            wprintf( L"elapsed ms %ws  /", perfTime.RenderDurationInMS( elapsed ) );
            wprintf( L"  cpu ms %ws  /", perfTime.RenderDurationInMS( ullU.QuadPart + ullK.QuadPart ) );
            wprintf( L"  core efficiency %.2lf%%  /", 100 * efficiency );
            wprintf( L"  cores used %.2lf", 100 * coresUsed );
            wprintf( L"\n" );
        }
        else
        {
            wprintf( L"kernel CPU:        %15ws\n", perfTime.RenderDurationInMS( ullK.QuadPart ) );
            wprintf( L"user CPU:          %15ws\n", perfTime.RenderDurationInMS( ullU.QuadPart ) );
            wprintf( L"total CPU:         %15ws\n", perfTime.RenderDurationInMS( ullU.QuadPart + ullK.QuadPart ) );
            wprintf( L"core efficiency:   %14.2lf%%\n", 100 * efficiency );
            wprintf( L"average cores used:%15.2lf\n", 100 * coresUsed );
            wprintf( L"elapsed time:      %15ws\n", perfTime.RenderDurationInMS( elapsed ) );
        }
    }
} //ShowProcessDetails

void Usage()
{
    wprintf( L"Usage: ti [-o] app [arg1] [arg2] [...]\n" );
    wprintf( L"  Timing Info starts a process then displays information about that process after it exits\n" );
    wprintf( L"      -o        Show timing info on one line instead of multiple\n" );

    exit( 1 );
} //Usage

extern "C" int __cdecl wmain( int argc, WCHAR * argv[] )
{
    try
    {
        bool fOneLine = false;

        if ( 1 == argc )
            Usage();

        WCHAR c = argv[1][0];

        if ( L'?' == c )
            Usage();

        int firstArg = 1;

        if ( L'-' == c || L'/' == c )
        {
            if ( 'O' == toupper( argv[1][1] ) )
            {
                fOneLine = true;
                firstArg = 2;

                if ( 2 == argc )
                    Usage();
            }
            else
                Usage();
        }

        WCHAR * pproc = g_awcProcess;

        for ( int arg = firstArg; arg < argc; arg++ )
        {
            // +4 to save space for NULL, two quotes, and a space.

            if ( ( wcslen( g_awcProcess ) + wcslen( argv[ arg ] ) + 4 ) >= _countof( g_awcProcess ) )
            {
                wprintf( L"argument list is too long\n" );
                Usage();
            }

            // Add a space between arguments

            if ( firstArg != arg )
                *pproc++ = L' ';

            // wrap arguments with spaces in double quotes

            bool space = ( NULL != wcschr( argv[ arg ], L' ' ) );

            if ( space )
                *pproc++ = L'"';

            WCHAR * parg = argv[ arg ];

            while ( 0 != *parg )
                *pproc++ = *parg++;

            if ( space )
                *pproc++ = L'"';
        }

        *pproc++ = 0;

        STARTUPINFO si;
        ZeroMemory( &si, sizeof si );
        si.cb = sizeof si;

        PROCESS_INFORMATION pi;
        ZeroMemory( &pi, sizeof pi );

        CPerfTime perfTime;
        BOOL ok = CreateProcess( NULL, g_awcProcess, 0, 0, FALSE, 0, 0, 0, &si, &pi );

        if ( ok )
        {
            g_awcApp[0] = 0;
            GetModuleFileNameEx( pi.hProcess, NULL, g_awcApp, _countof( g_awcApp ) );

            WaitForSingleObject( pi.hProcess, INFINITE );
  
            LONGLONG elapsed = 0;
            perfTime.CumulateSince( elapsed );

            ShowProcessDetails( pi.hProcess, elapsed, perfTime, fOneLine );

            CloseHandle( pi.hThread );
            CloseHandle( pi.hProcess );
        }
        else
        {
            wprintf( L"can't create process; getlast error: %#x (decimal %d)\n", GetLastError(), GetLastError() );
        }
    }
    catch( ... )
    {
        wprintf( L"caught an exception in %ws, exiting\n", argv[ 0 ] );
    }

    return 0;
} //main
