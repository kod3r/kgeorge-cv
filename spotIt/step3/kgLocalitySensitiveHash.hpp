/********************************************************************************/
//
//      author: koshy george, kgeorge2@gmail.com, copyright 2014
//      please read license.txt provided
//      free-bsd-license
/********************************************************************************/
#if !defined(KG_LOCALITY_SENSITIVE_HASH_)
#define KG_LOCALITY_SENSITIVE_HASH_

#include <iostream>
#include <vector>
#include <limits>
#include <deque>
#include <functional>
#include <unordered_set>
#include <set>
#include <string>
#include <functional>


#include "kgUtils.hpp"
#include "kgKernel.hpp"


/*
template< typename T >
struct KgLocalitySensitiveHash_Traits {
    typedef typename T::value_type I;
    typedef typename AppropriateNonIntegralType<I>::value_type K;
    static K distSqrd(const T &l, const T &r) {
        throw std::runtime_error( "not implemented" );
    }

    static T orthogonal(const T &) {
        throw std::runtime_error( "not implemented" );
    }

    static K dot(const T &l, const T &t) {
        throw std::runtime_error( "not implemented" );
    }
    static T &negate(T) {
        throw std::runtime_error( "not implemented" );
    }

    //returns  -1 for left, 0 for on , +1 for right
    static int leftRightOrOn(const T &a, const T &b, const T &c) {
        throw std::runtime_error( "not implemented" );
    }

    static T gen(std::normal_distribution<> &norm, std::mt19937 &gen) {
        throw std::runtime_error( "not implemented" );
    }

    friend T & operator * ( T &l, K s) {
        throw std::runtime_error( "not implemented" );
    }
};

*/

template< typename HashEntry>
struct HashTableValue {
    std::deque< HashEntry > data;
    void clear() {
        data.clear();
    }
};


template<typename T, typename HashEntry,  int numBuckets, typename TTraits = KgLocalitySensitiveHash_Traits< T >  >
struct Hash {
    typedef typename T::value_type I;
    typedef typename AppropriateNonIntegralType<I>::value_type K;
    static constexpr K zero = static_cast<K>(0);
    static constexpr K one = static_cast<K>(1);
    Hash(K w, K minRange, K maxRange):
        w(w),
        minRange(minRange),
        maxRange(maxRange),
        oneByW(one/w),
        gen(42),
        ndist(std::normal_distribution<>(zero, one)) {
        resize(w, minRange, maxRange);
    }
    Hash():
        w(1),
        oneByW(one/w),
        gen(42),
        minRange(-1),
        maxRange(1),
        ndist(std::normal_distribution<>(zero, one)) {
        resize(w, minRange, maxRange);
    }

    int findIndex( int ret[numBuckets]) {
        assert(numBuckets == 3);
        //please rewrite this function in a loop
        //for nBuckets != 3
        return ret[2] * nSizePerBucket * nSizePerBucket + ret[1] * nSizePerBucket + ret[0];
    }
    
    void index( const T &arg, const HashEntry & entry) {
        K temp;
        K numSizeBy2 = nSizePerBucket/2;
        for(int i=0; i < numBuckets; ++i) {
            temp = TTraits::dot(a[i], arg) + b[i];
            tempStorage[i] = floor(temp * oneByW);
            assert(tempStorage[i] >= -numSizeBy2 && tempStorage[i] <= numSizeBy2);
            tempStorage[i] += numSizeBy2;
            assert(tempStorage[i] > 0);
        }
        int hashTableIndex = findIndex(tempStorage);
        assert(hashTableIndex < hashTable.size());
        HashTableValue<HashEntry> &hashTableVal = hashTable[hashTableIndex];
        auto hit = hashTableVal.data.begin();
        int numEntriesOfSameValue =0;
        bool found = false;
        for(hit = hashTableVal.data.begin(); hit != hashTableVal.data.end(); ++hit) {
            if( *hit == entry ) {
                hit->count++;
                numEntriesOfSameValue++;
                found = true;
            }
        }
        if(!found) {
            HashEntry entry_2(entry);
            entry_2.count++;
            hashTableVal.data.push_back(entry_2);

            ++numEntries;
            if(((numEntries % 100) == 0) && (numEntries >= 100)) {
                //stats();
            }
        }
        assert( numEntriesOfSameValue <= 1);
    }
    
    
    void stats() const {
        std::map<int, int> histogram;
        for(auto hashTableValIt = hashTable.begin(); hashTableValIt != hashTable.end(); ++hashTableValIt ) {
            int n2 = 0;
            for(auto hoit = hashTableValIt->data.begin(); hoit!= hashTableValIt->data.end(); ++hoit ) {
                n2 += hoit->count;
            }
            n2 /= 10;
            auto histIt = histogram.find(n2);
            if (histIt == histogram.end()) {
                histogram.insert( std::pair<int, int>(n2, 1));
            } else {
                histIt->second += 1;
            }
        }
        std::cout << "histogram of hashTbale" << std::endl;
        for(auto mit = histogram.begin(); mit != histogram.end(); ++mit) {
            std::cout << mit->first << ", " << mit->second << std::endl;
        }
    }

    void clear() {
        for(auto hit = hashTable.begin(); hit != hashTable.end(); ++hit ) {
            hit->clear();
        }
        numEntries=0;
    }

    void serialize(const std::string &filename) {
        cv::FileStorage fs(filename, cv::FileStorage::WRITE);
        fs << "hashTable" << "{";
        fs << "description" << "geometric hash";
        fs << "numBuckets" << numBuckets;
        fs << "minRange" << minRange;
        fs << "maxRange" << maxRange;
        fs << "windowSize" << w;
        fs << "numEntries" << numEntries;
        fs << "data" << "[";
        for(int i=0; i < hashTable.size(); ++i) {
            const HashTableValue<HashEntry> &htValue = hashTable[i];
            if (htValue.data.size() > 0) {
                fs << "{";
                fs << "index" << i;
                fs << "tableEntries" << "[";
                for(int j=0; j < htValue.data.size(); ++j) {
                    const HashEntry &he = htValue.data[j];
                    fs << he;
                }
                fs << "]";
                fs << "}";

            }
        }
        fs << "]";
        fs << "}";
    }

    void unSerialize(const std::string &filename) {
        cv::FileStorage fs(filename, cv::FileStorage::READ);
        if (!fs.isOpened())
        {
            std::cerr << "Failed to open " << filename << std::endl;
            throw std::runtime_error("failed to open file");
        }
        const cv::FileNode &fn = fs["hashTable"];
        
        const std::string description( (std::string)fn["description"]);
        int nBuckets_2 = (int)fn["numBuckets"];
        K minRange = (K)fn["minRange"];
        K maxRange = (K)fn["maxRange"];
        int numEntriesReported = (int)fn["numEntries"];
        K w = (K)fn["windowSize"];
        resize(w, minRange, maxRange);
        
        cv::FileNode data = fn["data"];                         // Read string sequence - Get node
        if (data.type() != cv::FileNode::SEQ)
        {
            std::cerr << "data is not a sequence! FAIL" << std::endl;
            throw std::runtime_error("data is not a sequence! FAIL");
        }
        
        cv::FileNodeIterator it = data.begin(), it_end = data.end(); // Go through the node

        for (; it != it_end; ++it) {
            const cv::FileNode &thisNode = *it;
            int index = (int) thisNode["index"];
            const cv::FileNode &tableEntries = thisNode["tableEntries"];
            if (tableEntries.type() != cv::FileNode::SEQ)
            {
                std::cerr << "tableEntries is not a sequence! FAIL" << std::endl;
                throw std::runtime_error("tableEntries is not a sequence! FAIL");
            }

            cv::FileNodeIterator it_2 = tableEntries.begin(), it_2_end = tableEntries.end(); // Go through the node
         
            
            for (; it_2 != it_2_end; ++it_2) {
                HashEntry he;
                (*it_2) >> he;
                hashTable[index].data.push_back(he);
                ++numEntries;
            }
        }
        assert(numEntriesReported == numEntries);
    }

    protected:
     void resize( K w_arg, K minRange_arg, K maxRange_arg ) {

        clear();
        assert( minRange_arg == -(maxRange_arg)); //right now this has to be likethis
        w = w_arg;
        assert(w > zero);
        assert(maxRange_arg > minRange_arg);
        maxRange = maxRange_arg;
        minRange = minRange_arg;
        nSizePerBucket = ceil((maxRange - minRange)/w);

        assert(nSizePerBucket > 1);

        oneByW = one / w;
        nSizeAllBuckets = nSizePerBucket*nSizePerBucket*nSizePerBucket;
        gen = std::mt19937(42);
        udist = std::uniform_real_distribution<K>(zero, w);
        hashTable.resize(nSizeAllBuckets);
        for(int i=0; i < numBuckets; ++i) {
            a[i] = TTraits::gen(ndist, gen);
            b[i] = udist(gen);
        }
     }
    public:
    std::normal_distribution<> ndist;
    std::uniform_real_distribution<K> udist;
    std::mt19937 gen;
    T a[numBuckets];
    K b[numBuckets];
    K w;
    K oneByW;
    K minRange;
    K maxRange;
    int tempStorage[numBuckets];
    int nSizePerBucket;
    int nSizeAllBuckets;
    std::deque<HashTableValue< HashEntry > > hashTable;
    int numEntries;
};

#endif //KG_LOCALITY_SENSITIVE_HASH_