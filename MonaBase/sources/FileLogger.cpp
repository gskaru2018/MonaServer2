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

#include "Mona/FileLogger.h"
#include "Mona/Logs.h"


using namespace std;

namespace Mona {

FileLogger::FileLogger(const string dir, UInt32 sizeByFile, UInt16 rotation) : _sizeByFile(sizeByFile), _rotation(rotation),
	_pFile(new File(Path(MAKE_FOLDER(dir), "0.log"), File::MODE_APPEND)) {
	_written = range<UInt32>(_pFile->size());
}

FileLogger& FileLogger::log(LOG_LEVEL level, const Path& file, long line, const string& message) {
	static string Buffer; // max size controlled by Logs system!
	static string temp;
	static Exception ex;

	String::Assign(Buffer, String::Date("%d/%m %H:%M:%S.%c  "), Logs::LevelToString(level));
	Buffer.append(25 - Buffer.size(), ' ');

	String::Append(Buffer, Thread::CurrentId(), ' ');
	if (String::ICompare(FileSystem::GetName(file.parent(), temp), "sources") != 0 && String::ICompare(temp, "mona") != 0)
		String::Append(Buffer, temp, '/');
	String::Append(Buffer, file.name(), '[', line, "] ");
	if (Buffer.size() < 60)
		Buffer.append(60 - Buffer.size(), ' ');
	String::Append(Buffer, message, '\n');

	if (_pFile->write(ex, Buffer.data(), Buffer.size()))
		manage(Buffer.size());
	else
		_pFile.reset();
	return self;
}

FileLogger& FileLogger::dump(const string& header, const UInt8* data, UInt32 size) {
	String buffer(String::Date("%d/%m %H:%M:%S.%c  "), header, '\n');
	Exception ex;
	if (_pFile->write(ex, buffer.data(), buffer.size()) || !_pFile->write(ex, data, size))
		manage(buffer.size() + size);
	else
		_pFile.reset();
	return self;
}

void FileLogger::manage(UInt32 written) {
	if (!_sizeByFile || (_written += written) <= _sizeByFile) // don't use _pLogFile->size(true) to avoid disk access on every file log!
		return; // _logSizeByFile==0 => inifinite log file! (user choice..)

	_written = 0;
	_pFile.reset(new File(Path(_pFile->parent(), "0.log"), File::MODE_WRITE)); // override 0.log file!

	// delete more older file + search the older file name (usefull when _rotation==0 => no rotation!) 
	string name;
	UInt32 maxNum(0);
	Exception ex;
	FileSystem::ForEach forEach([this, &ex, &name, &maxNum](const string& path, UInt16 level) {
		UInt16 num = String::ToNumber<UInt16, 0>(FileSystem::GetBaseName(path, name));
		if (_rotation && num >= (_rotation - 1))
			FileSystem::Delete(ex, String(_pFile->parent(), num, ".log"));
		else if (num > maxNum)
			maxNum = num;
	});
	FileSystem::ListFiles(ex, _pFile->parent(), forEach);
	// rename log files
	do {
		FileSystem::Rename(String(_pFile->parent(), maxNum, ".log"), String(_pFile->parent(), maxNum + 1, ".log"));
	} while (maxNum--);

}

} // namespace Mona
