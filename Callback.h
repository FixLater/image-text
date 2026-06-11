#pragma once

class Listener {
public:  
	virtual void callback(int code, const wchar_t* path = NULL) = 0;

};