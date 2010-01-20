/* Declarations file memorystreammanager.hpp
	
	Copyright 2010 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_MEMORY_STREAM_MANAGER_HPP
#define CS_MEMORY_STREAM_MANAGER_HPP

#include "utility/streampointer.hpp"

#include "foundation/object.hpp"

#include "foundation/common.hpp"

class IOStream;

class Mutex;

namespace foundation{

class RequestUid;

class MemoryStreamManager: public Object{
public:
	MemoryStreamManager();
	~MemoryStreamManager();
	int stream(
		StreamPointer<IOStream> &_sptr,
		const RequestUid &_rrequid,
		uint32 _flags = 0
	);
private:
	struct Data;
	Data	&d;
};

}//namespace foundation

#endif
