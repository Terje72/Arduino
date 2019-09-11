#ifndef __HTTP_VALUE_H__
#define __HTTP_VALUE_H__


namespace WebServerDarkain {

typedef struct {
	const	char	*key;
	const	char	*value;
} HTTPKeyValue;




class HTTPValue {
	public:



	////////////////////////////////////////////////////////////////////////////
	// CONSTRUCTOR
	////////////////////////////////////////////////////////////////////////////
	inline HTTPValue() {
		items = nullptr;
		count = 0;
	}



	////////////////////////////////////////////////////////////////////////////
	// DESTRUCTOR
	////////////////////////////////////////////////////////////////////////////
	inline ~HTTPValue() {
		reset();
	}




	////////////////////////////////////////////////////////////////////////////
	// PROCESS THE BUFFER
	////////////////////////////////////////////////////////////////////////////
	char *process(char *buffer, bool clear=true) {
		auto id = (clear ? 0 : total());
		allocate(_count(buffer), clear);
		return _parse(buffer, id);
	}




	////////////////////////////////////////////////////////////////////////////
	// DELETE ALL VALUE POINTERS
	////////////////////////////////////////////////////////////////////////////
	inline void reset() {
		free(items);
		items = nullptr;
		count = 0;
	}




	////////////////////////////////////////////////////////////////////////////
	// ALLOCATE A NEW BUFFER
	// THIS DETECTS ALLOCATION ERRORS NOW
	// ITEM LIST ASSUMED EMPTY IF CANNOT ALLOCATE POINTER BUFFER
	////////////////////////////////////////////////////////////////////////////
	inline void allocate(int total, bool clear=true) {
		if (clear) reset();

		total				+= count;
		int size			 = sizeof(HTTPKeyValue) * total;
		HTTPKeyValue *tmp	 = items;
		items				 = (HTTPKeyValue*) realloc(items, size);

		if (items) {
			for (auto i=count; i<total; i++) {
				items[i].key	= nullptr;
				items[i].value	= nullptr;
			}
			count = total;

		} else {
			items = tmp;
		}
	}




	////////////////////////////////////////////////////////////////////////////
	// GET TOTAL NUMBER OF VALUES IN OUR CONTROL
	////////////////////////////////////////////////////////////////////////////
	inline int total() const {
		return count;
	}




	////////////////////////////////////////////////////////////////////////////
	// GET THE KEY OF AN ITEM
	////////////////////////////////////////////////////////////////////////////
	inline const char *key(int id) const {
		auto item = get(id);
		return item ? item->key : nullptr;
	}




	////////////////////////////////////////////////////////////////////////////
	// CHECK IF A VALUE KEY EXISTS (C-STRING)
	////////////////////////////////////////////////////////////////////////////
	inline bool has(const char *key) const {
		return !!get(key);
	}




	////////////////////////////////////////////////////////////////////////////
	// CHECK IF A VALUE KEY EXISTS (ARDUINO STRING)
	////////////////////////////////////////////////////////////////////////////
	inline bool has(String key) const {
		return !!get(key);
	}




	////////////////////////////////////////////////////////////////////////////
	// GET A VALUE OBJECT BASED ON ID
	////////////////////////////////////////////////////////////////////////////
	inline HTTPKeyValue *get(int id) const {
		if (id >= total()  ||  id < 0) return nullptr;
		return &items[id];
	}




	////////////////////////////////////////////////////////////////////////////
	// GET A VALUE OBJECT BASED ON KEY (C-STRING)
	////////////////////////////////////////////////////////////////////////////
	HTTPKeyValue *get(const char *key) const {
		if (key == nullptr  ||  *key == 0) return nullptr;

		for (auto i=0; i<total(); i++) {
			if (strcasecmp(items[i].key, key) == 0) {
				return &items[i];
			}
		}

		return nullptr;
	}




	////////////////////////////////////////////////////////////////////////////
	// GET A VALUE OBJECT BASED ON KEY (ARDUINO STRING)
	////////////////////////////////////////////////////////////////////////////
	inline HTTPKeyValue *get(String key) const {
		return get( (const char *) key.c_str() );
	}




	////////////////////////////////////////////////////////////////////////////
	// GET A VALUE BASED ON INDEX ID
	////////////////////////////////////////////////////////////////////////////
	inline const char *value(int id) const {
		auto item = get(id);
		return item ? item->value : nullptr;
	}




	////////////////////////////////////////////////////////////////////////////
	// GET A VALUE BASED ON KEY (C-STRING)
	////////////////////////////////////////////////////////////////////////////
	inline const char *value(const char *key) const {
		auto item = get(key);
		return item ? item->value : nullptr;
	}




	////////////////////////////////////////////////////////////////////////////
	// GET A VALUE BASED ON KEY (ARDUINO STRING)
	////////////////////////////////////////////////////////////////////////////
	inline const char *value(String key) const {
		auto item = get(key);
		return item ? item->value : nullptr;
	}




	////////////////////////////////////////////////////////////////////////////
	// GET AN INTEGER VALUE BASED ON INDEX ID
	////////////////////////////////////////////////////////////////////////////
	inline int integer(int id) const {
		auto item = get(id);
		return item ? atoi(item->value) : 0;
	}




	////////////////////////////////////////////////////////////////////////////
	// GET AN INTEGER VALUE BASED ON KEY (C-STRING)
	////////////////////////////////////////////////////////////////////////////
	inline int integer(const char *key) const {
		auto item = get(key);
		return item ? atoi(item->value) : 0;
	}




	////////////////////////////////////////////////////////////////////////////
	// GET AN INTEGER VALUE BASED ON KEY (ARDUINO STRING)
	////////////////////////////////////////////////////////////////////////////
	inline int integer(String key) const {
		auto item = get(key);
		return item ? atoi(item->value) : 0;
	}




	////////////////////////////////////////////////////////////////////////////
	// STATIC VERSION OF SETTING THE VALUE
	////////////////////////////////////////////////////////////////////////////
	inline void set(int id, const char *key, const char *value=nullptr) {
		auto item = get(id);
		if (item) {
			item->key	= key;
			item->value	= value;
		}
	}




	////////////////////////////////////////////////////////////////////////////
	// !!! WARNING !!! EXTREMELY DANGEROUS
	// DON'T DO THIS UNLESS YOU ABSOLUTELY KNOW WHAT YOURE DOING!
	////////////////////////////////////////////////////////////////////////////
	// INCREASE OR DECREASE THE POINTER OFFSET
	////////////////////////////////////////////////////////////////////////////
	void __offset(int offset) {
		if (offset == 0) return;
		for (auto i=0; i<total(); i++) {
			if (items[i].key)	items[i].key	+= offset;
			if (items[i].value)	items[i].value	+= offset;
		}
	}




	////////////////////////////////////////////////////////////////////////////
	// CALCULATE THE TOTAL NUMBER OF PARAM VALUES
	////////////////////////////////////////////////////////////////////////////
	protected:
	virtual int _count(const char *buffer) const = 0;




	////////////////////////////////////////////////////////////////////////////
	// PARSE AND TOKENIZE THE BUFFER
	////////////////////////////////////////////////////////////////////////////
	protected:
	virtual char *_parse(char *buffer, int id=0) = 0;




	////////////////////////////////////////////////////////////////////////////
	// MEMBER VARIABLES
	////////////////////////////////////////////////////////////////////////////
	protected:
	int				 count;
	HTTPKeyValue	*items;
};

}; // namespace WebServerDarkain


#endif // __HTTP_VALUE_H__
