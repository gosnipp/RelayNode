#ifndef _RELAY_FLAGGEDARRAYSET_H
#define _RELAY_FLAGGEDARRAYSET_H

#include <vector>
#include <thread>
#include <map>
#include <unordered_map>

#include "utils.h"

/******************************
 **** FlaggedArraySet util ****
 ******************************/
struct ElemAndFlag {
	bool flag;
	std::shared_ptr<std::vector<unsigned char> > elem;
	std::shared_ptr<std::vector<unsigned char> > elemHash;
	bool allowDups;
	ElemAndFlag(const std::shared_ptr<std::vector<unsigned char> >& elemIn, bool flagIn, bool allowDupsIn, bool setHash) :
		flag(flagIn), elem(elemIn), allowDups(allowDupsIn)
	{
		if (setHash) {
			elemHash = std::make_shared<std::vector<unsigned char> >(32);
			double_sha256(&(*elem)[0], &(*elemHash)[0], elem->size());
		}
	}
	bool operator == (const ElemAndFlag& o) const {
		if (allowDups)
			return o.elem == elem;
		bool hashSet = o.elemHash && elemHash;
		return o.elem == elem ||
			(hashSet && *o.elemHash == *elemHash) ||
			(!hashSet && *o.elem == *elem);
	}
};

namespace std {
	template <> struct hash<ElemAndFlag> {
		size_t operator()(const ElemAndFlag& e) const {
			// We duplicate the shared_ptr because we may not actually be entirely thread-safe here, otherwise
			const std::shared_ptr<std::vector<unsigned char> > ptr(e.elem);
			const std::vector<unsigned char>& v = *ptr;

			if (v.size() < 5 + 32 + 4)
				return 42; // WAT?
			size_t res = 0;
			static_assert(sizeof(size_t) == 4 || sizeof(size_t) == 8, "Your size_t is neither 32-bit nor 64-bit?");
			for (unsigned int i = (5 + 32 + 4) - 8; i < 5 + 32 + 4; i += sizeof(size_t)) {
				for (unsigned int j = 0; j < sizeof(size_t); j++)
					res ^= v[i+j] << 8*j;
			}
			return res;
		}
	};
}


class Deduper;

class FlaggedArraySet {
private:
	unsigned int maxSize, flag_count;
	uint64_t offset;
	std::unordered_map<ElemAndFlag, uint64_t> backingMap;
	std::vector<std::unordered_map<ElemAndFlag, uint64_t>::iterator> indexMap;
	bool allowDups;

	// The mutex is only used by memory deduper, FlaggedArraySet is not thread-safe
	// It is taken by changes to backingMap, any touches to backingMap in the deduper thread, or any touches to elem
	friend class Deduper;
	mutable WaitCountMutex mutex;

public:
	void clear();
	FlaggedArraySet(unsigned int maxSizeIn, bool allowDupsIn);
	~FlaggedArraySet();

	size_t size() const { return backingMap.size(); }
	size_t flagCount() const { return flag_count; }
	bool contains(const std::shared_ptr<std::vector<unsigned char> >& e) const;

	FlaggedArraySet& operator=(const FlaggedArraySet& o) {
		maxSize = o.maxSize;
		flag_count = o.flag_count;
		offset = o.offset;
		backingMap = o.backingMap;
		indexMap = o.indexMap;
		allowDups = o.allowDups;
		return *this;
	}

private:
	void remove_(size_t index);

public:
	void add(const std::shared_ptr<std::vector<unsigned char> >& e, bool flag);
	int remove(const std::shared_ptr<std::vector<unsigned char> >& e);
	bool remove(int index, std::shared_ptr<std::vector<unsigned char> >& elem, std::shared_ptr<std::vector<unsigned char> >& elemHash);

	void for_all_txn(const std::function<void (const std::shared_ptr<std::vector<unsigned char> >&)> callback) const;
};

#endif
