/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#include "Mona/Util.h"
#include "Mona/File.h"
#if !defined(_WIN32)
#include <sys/times.h>
	#include <unistd.h>
	#include <sys/syscall.h>
	extern "C" char **environ;
#endif

using namespace std;

namespace Mona {

const char* Util::_URICharReserved("%<>{}|\\\"^`#?\x7F");

const char Util::_B64Table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const char Util::_ReverseB64Table[128] = {
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
	64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
	64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
};

const UInt8 Util::UInt8Generators[] = {
	 0,   1,  1,  2,  1,  3,  5,  4,  5,  7,  7,  7,  7,  8,  9,  8,
	 11, 11, 11, 12, 11, 13, 15, 14, 17, 14, 15, 17, 17, 18, 19, 19,
	 21, 20, 21, 22, 23, 23, 23, 23, 27, 25, 25, 27, 27, 28, 27, 29,
	 31, 30, 31, 32, 31, 33, 31, 34, 37, 35, 37, 36, 37, 38, 37, 40,
	 41, 41, 41, 41, 41, 43, 43, 44, 43, 45, 47, 46, 47, 48, 47, 49,
	 49, 50, 51, 51, 53, 53, 53, 55, 53, 55, 53, 55, 57, 56, 57, 59,
	 59, 60, 61, 61, 63, 62, 61, 64, 63, 64, 67, 66, 67, 67, 69, 70,
	 69, 70, 71, 71, 73, 71, 73, 74, 73, 75, 75, 76, 77, 77, 79, 78,
	 79, 80, 79, 81, 83, 82, 83, 83, 83, 85, 85, 86, 87, 86, 89, 87,
	 89, 91, 89, 92, 91, 92, 91, 93, 93, 95, 95, 96, 95, 97, 99, 98,
	 99,100,101,101,101,103,103,103,103,103,103,106,105,107,109,108,
	109,109,109,111,109,112,111,113,113,114,115,116,115,118,117,118,
	119,119,121,121,121,122,125,123,123,124,125,125,125,127,127,128,
	129,129,131,130,131,133,131,133,133,134,135,134,137,137,137,138,
	137,139,141,140,139,142,141,142,143,144,145,144,147,146,149,148,
	149,149,151,149,151,151,151,153,153,154,153,155,157,156,157,158
};

const Parameters& Util::Environment() {
	static struct Environment : Parameters, virtual Object {
		Environment() {
			const char* line(*environ);
			for (UInt32 i = 0; (line = *(environ + i)); ++i) {
				const char* value = strchr(line, '=');
				if (value) {
					string name(line, (value++) - line);
					setString(name, value);
				} else
					setString(line, NULL);
			}
		}
	} Environment;
	return Environment;
}

UInt64 Util::Random() {
	// flip+process Id to make variate most signifiant bit on different computer AND on same machine
	static UInt64 A = Byte::Flip64(Process::Id()) | Byte::Flip64(Time::Now());
#if defined(_WIN32)
#if (_WIN32_WINNT >= 0x0600)
	static UInt64 B = Byte::Flip64(Process::Id()) | Byte::Flip64(GetTickCount64());
#else
	static UInt64 B = Byte::Flip64(Process::Id()) | Byte::Flip32(GetTickCount());
#endif
#else
	static tms TMS;
	static UInt64 B = Byte::Flip64(Process::Id()) | Byte::Flip32(times(&TMS));
#endif
	// xorshift128plus = faster algo able to pass BigCrush test
	UInt64 x = A;
	UInt64 const y = B;
	A = y; // not protect A and B to go more faster (and useless on random value...)
	x ^= x << 23; // a
	B = x ^ y ^ (x >> 17) ^ (y >> 26); // b, c
	return B + y; // cast gives modulo here!
}

size_t Util::UnpackUrl(const char* url, string& address, string& path, string& query) {
	
	path.clear();
	query.clear();


	const char* dot(strpbrk(url, ":/\\"));
	if (!dot) {
		path.assign("/").append(url);
		return 1; // is file!
	}

	
	if (*dot == ':') {
		// protocol://address
		++dot;
		while (*dot && (*dot == '/' || *dot == '\\'))
			++dot;
		if (!*dot) // no address, no path, just "scheme://"
			return string::npos;
		const char* itEnd(dot);
		while (*itEnd && *itEnd != '/' && *itEnd != '\\' && *itEnd != '?')
			++itEnd;
		address.assign(dot, itEnd);
		url = itEnd; // on slash after address
	}

	// Decode PATH

	vector<size_t> slashs;

	// level = 0 => path => next char
	// level = 1 => path => /
	// level > 1 => path => . after /
	UInt8 level(1); 

	ForEachDecodedChar forEach([&level,&slashs,&path,&query](char c,bool wasEncoded){

		if (c == '?')
			return false;

		// path

		if (c == '/' || c == '\\') {

			// level + 1 = . level
			if (level > 2) {
				// /../
				if (!slashs.empty()) {
					path.resize(slashs.back());
					slashs.pop_back();
				} else
					path.clear();
			}
			level = 1;

			return true;
		}
		
		if (level) {
			// level = 1 or 2
			if (level<3 && c == '.') {
				++level;
				return true;
			}
			slashs.emplace_back(path.size());
			path.append("/..",level);
			level = 0;
		}

		// Add current character
		path += c;

		return true;
	});

	url += DecodeURI(url,forEach);

	// get QUERY
	if (*url)
		query.assign(url);

	if (level) {
		if (level > 2) // /..
			path.resize(slashs.empty() ? 0 : slashs.back());
		return string::npos;
	}
	return slashs.back()+1; // can't be empty here!
}

Parameters& Util::UnpackQuery(const char* query, size_t count, Parameters& parameters) {
	ForEachParameter forEach([&parameters](const string& key, const char* value) {
		parameters.setString(key, value ? value : "");
		return true;
	});
	UnpackQuery(query, count, forEach);
	return parameters;
}

UInt32 Util::UnpackQuery(const char* query, size_t count, const ForEachParameter& forEach) {
	Int32 countPairs(0);
	string name,value;
	bool isName(true);

	ForEachDecodedChar forEachDecoded([&isName, &countPairs, &name, &value, &forEach](char c, bool wasEncoded) {

		if (!wasEncoded) {
			if (c == '&') {
				++countPairs;
				if (!forEach(name, isName ? NULL : value.c_str())) {
					countPairs *= -1;
					return false;
				}
				isName = true;
				value.clear();
				name.clear();
				return true;
			}
			if (c == '=' && isName) {
				isName = false;
				return true;
			}
			// not considerate '+' char to replace by ' ', because it affects Base64 value in query which includes '+',
			// a space must be in a legal %20 format!
		}
		if (isName) {
			if(countPairs || c != '?') // ignore first '?'!
				name += c;
		} else
			value += c; 
		return true;
	});

	if (DecodeURI(query, count, forEachDecoded) && countPairs>=0) {
		// for the last pairs just if there was some decoded bytes
		++countPairs;
		forEach(name, isName ? NULL : value.c_str());
	}

	return abs(countPairs);
}

UInt32 Util::DecodeURI(const char* value, std::size_t count, const ForEachDecodedChar& forEach) {

	const char* begin(value);

	while (count && (count!=string::npos || *value)) {

		if (*value == '%') {
			// %
			++value;
			if(count!=string::npos)
				--count;
			if (!count || (count==string::npos && !*value)) {
				 // syntax error
				if (!forEach('%', false))
					--value;
				return value-begin;
			}
			
			char hi = toupper(*value);
			++value;
			if(count!=string::npos)
				--count;
			if (!count || (count==string::npos && !*value)) {
				// syntax error
				if (forEach('%', false)) {
					if (!forEach(hi, false))
						--value;
				} else
					value-=2;
				return value-begin;
			}
			char lo = toupper(*value++);
			if (count != string::npos)
				--count;
			if (!isxdigit(lo) || !isxdigit(hi)) {
				// syntax error
				if (forEach('%', false)) {
					if (forEach(hi, false)) {
						if (forEach(lo, false))
							continue;
					} else
						--value;
				} else
					value -= 2;
				return value - begin;
			}
			if (forEach(char((hi - (hi <= '9' ? '0' : '7')) << 4) | ((lo - (lo <= '9' ? '0' : '7')) & 0x0F), true))
				continue;
			return value - 3 - begin;
		}
		if (!forEach(*value, false))
			break;
		++value;
		if (count != string::npos)
			--count;
	}

	return value - begin;
}

void Util::Dump(const UInt8* data, UInt32 size, Buffer& buffer) {
	UInt8 b;
	UInt32 c(0);
	buffer.resize((UInt32)ceil((double)size / 16) * 67, false);

	const UInt8* end(data + size);
	UInt8*		 out(buffer.data());

	while (data < end) {
		c = 0;
		*out++ = '\t';
		while ((c < 16) && (data < end)) {
			b = *data++;
			snprintf((char*)out, 4, "%X%X ", b >> 4, b & 0x0f);
			out += 3;
			++c;
		}
		data -= c;
		while (c++ < 16) {
			memcpy((char*)out, "   \0", 4);
			out += 3;
		}
		*out++ = ' ';
		c = 0;
		while ((c < 16) && (data < end)) {
			b = *data++;
			if (b > 31)
				*out++ = b;
			else
				*out++ = '.';
			++c;
		}
		while (c++ < 16)
			*out++ = ' ';
		*out++ = '\n';
	}
}



bool Util::ReadIniFile(const string& path, Parameters& parameters) {
	Exception ex;
	File file(path, File::MODE_READ);
	if (!file.load(ex))
		return false;
	UInt32 size = range<UInt32>(file.size());
	if (size == 0)
		return true;
	Buffer buffer(size);
	if (file.read(ex, buffer.data(), size) < 0)
		return false;

	char* cur = STR buffer.data();
	const char* end = cur+size;
	const char* key, *value;
	string section;
	while (cur < end) {
		// skip space
		while (isspace(*cur) && ++cur < end);
		if (cur == end)
			break;

		// skip comments
		if (*cur == ';') {
			while (++cur < end && *cur != '\n');
			++cur;
			continue;
		}
		
		// line
		key = cur;
		value = NULL;
		size_t vSize(0), kSize(0);
		const char* quote=NULL;
		do {
			if (*cur == '\n')
				break;
			if (*cur == '\'' || *cur == '"') {
				if (quote) {
					if(*quote == *cur && quote < value) {
						// fix value!
						kSize += vSize + 1;
						value = NULL;
						vSize = 0;
					} // else was not a quote!
					quote = NULL;
				} else
					quote = cur;
			}
			if (value)
				++vSize;
			else if (*cur == '=')
				value = cur+1;
			else
				++kSize;
		} while (++cur < end);

		if (vSize)
			vSize = String::Trim(value, vSize);

		bool isSection = false;
		if (kSize) {
			kSize = String::TrimRight(key, kSize);

			if(kSize>1) {
				if (*key == '[' && ((vSize && value[vSize - 1] == ']') || (!value && key[kSize - 1] == ']'))) {
					// section
					// remove [
					--kSize;
					++key;
					// remove ]
					if (value)
						--vSize;
					else
						--kSize;
					isSection = true;
				}

				// remove quote on key
				if (key[0] == key[kSize - 1] && (key[0] == '"' || key[0] == '\'')) {
					kSize -= 2;
					++key;
				}
			}
		}

		if (vSize) {
			vSize = String::Trim(value, vSize);
			// remove quote on value
			if (vSize > 1 && value[0] == value[vSize - 1] && (value[0] == '"' || value[0] == '\'')) {
				vSize-=2;
				++value;
			}
		}

		if (isSection) {
			parameters.setString(section.assign(key, kSize), value, vSize);
			section += '.';
		} else
			parameters.setString(string(section).append(key, kSize), value, vSize);
	}

	return true;
}



} // namespace Mona
