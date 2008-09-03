/* Declarations file idtypemap.hpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#ifndef ID_TYPE_MAP_HPP
#define ID_TYPE_MAP_HPP

#include <string>

#include "system/debug.hpp"
#include "system/common.hpp"

#include "basetypemap.hpp"

namespace serialization{

template <class S>
S& operator&(uint32 &_t, S &_s);

class IdTypeMap: public BaseTypeMap{
public:
	IdTypeMap();
	~IdTypeMap();
	//its a dummy implementation
	static IdTypeMap* the(){
		return NULL;
	}
	/*virtual*/ void insert(FncTp, unsigned _pos, const char *, unsigned _maxpos);
	template <class Ser>
	void storeTypeId(Ser &_rs, const char *_name, std::string &_rstr, ulong _serid, void *_p){
		FncTp pf;
		uint32 &rul(getFunction(pf, _name, _rstr, _serid));
		if(pf){
			(*pf)(_p, &_rs, NULL);
		}
		idbgx(Dbg::ser_bin, ""<<rul);
		_rs.push(rul, "type_id");
		//return pf;
	}
	template <class Des>
	void parseTypeIdPrepare(Des &_rd, std::string &_rstr){
		uint32 *pu = reinterpret_cast<uint32*>(const_cast<char*>(_rstr.data()));
		idbgx(Dbg::ser_bin, ""<<*pu);
		_rd.push(*pu, "type_id");
	}
	FncTp parseTypeIdDone(const std::string &_rstr, ulong _serid);
private:
	uint32 & getFunction(FncTp &_rpf, const char *_name, std::string &_rstr, ulong _serid);
	struct Data;
	Data	&d;
};

}

#endif
