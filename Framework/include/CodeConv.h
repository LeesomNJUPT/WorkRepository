#ifndef _CODE_CONV_H
#define _CODE_CONV_H

#include <iostream>
#include <codecvt>
#include <memory>
#include <locale>
#include <codecvt>

using namespace std;

wstring ANSIToUnicode(const string &str);
string UnicodeToANSI(const wstring &wstr);
string UnicodeToUTF8(const wstring &wstr);
wstring UTF8ToUnicode(const string &str);
string UTF8ToANSI(const string &str);
string ANSIToUTF8(const string &str);

#endif