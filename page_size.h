#pragma once

struct PAGE_SIZE_INFO
{
    const _TCHAR *m_name;
    double m_width, m_height; // in millemeters
};

inline bool
page_size_parse(const _TCHAR *arg, double *width, double *height)
{
    static PAGE_SIZE_INFO s_page_size_info[] =
    {
        // A0...A10
        { _T("A0"), 1189, 841 },
        { _T("A1"), 841, 594 },
        { _T("A2"), 594, 420 },
        { _T("A3"), 420, 297 },
        { _T("A4"), 297, 210 },
        { _T("A5"), 210, 148 },
        { _T("A6"), 148, 105 },
        { _T("A7"), 105, 74 },
        { _T("A8"), 74, 52 },
        { _T("A9"), 52, 37 },
        { _T("A10"), 37, 26 },
        // B0...B10
        { _T("B0"), 1456, 1030 },
        { _T("B1"), 1030, 728 },
        { _T("B2"), 728, 515 },
        { _T("B3"), 515, 364 },
        { _T("B4"), 364, 257 },
        { _T("B5"), 257, 182 },
        { _T("B6"), 182, 128 },
        { _T("B7"), 128, 91 },
        { _T("B8"), 91, 64 },
        { _T("B9"), 64, 45 },
        { _T("B10"), 45, 32 },
        // Letter, Legal etc.
        { _T("Letter"), 279, 216 },
        { _T("Legal"), 356, 216 },
        { _T("Tabloid"), 432, 279 },
        { _T("Ledger"), 279, 432 },
        { _T("Junior Legal"), 127, 203 },
        { _T("Half Letter"), 140, 216 },
        { _T("Government Letter"), 203, 267 },
        { _T("Government Legal"), 216, 330 },
        // ANSI sizes
        { _T("ANSI A"), 216, 279 },
        { _T("ANSI B"), 279, 432 },
        { _T("ANSI C"), 432, 559 },
        { _T("ANSI D"), 559, 864 },
        { _T("ANSI E"), 864, 1118 },
        // Arch sizes
        { _T("Arch A"), 229, 305 },
        { _T("Arch B"), 305, 457 },
        { _T("Arch C"), 457, 610 },
        { _T("Arch D"), 610, 914 },
        { _T("Arch E"), 914, 1219 },
        { _T("Arch E1"), 762, 1067 },
        { _T("Arch E2"), 660, 965 },
        { _T("Arch E3"), 686, 991 },
    };

    for (auto& entry : s_page_size_info)
    {
        if (_tcsicmp(arg, entry.m_name) == 0)
        {
            *width = entry.m_width;
            *height = entry.m_height;
            return true;
        }
    }

    if (_stscanf(arg, _T("%lfx%lf"), width, height) != 2)
        return false;

    return *width > 0 && std::isnormal(*width) && *height > 0 && std::isnormal(*height);
}
