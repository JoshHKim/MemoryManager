#include "MemoryManager.h"
using namespace std;

    MemoryManager::MemoryManager(unsigned wordSize, std::function<int(int, void *)> allocator){
        initialized = false;
        hasShutdown = false;
        this->wordSize=wordSize;
        this->allocator=allocator;
    }
    MemoryManager::~MemoryManager(){
        shutdown();
    }
    void MemoryManager::initialize(size_t sizeInWords){
        if(initialized)
            shutdown();
        if(sizeInWords>65535)
            return;
        initialized = true;
        hasShutdown = false;
        m_start = malloc(sizeInWords*wordSize);
        numBlocks = sizeInWords;
        Block temp(m_start, false, sizeInWords);
        v_blocks.push_back(temp);
    }   
    void MemoryManager::shutdown(){
        if(!hasShutdown){
            std::free(m_start);
            hasShutdown = true;
            v_blocks.clear();
        }
    }
    void *MemoryManager::allocate(size_t sizeInBytes){
        int sizeInWords = sizeInBytes/wordSize;
        if(sizeInBytes>sizeInWords*wordSize)
            sizeInWords++;
        void* tempList = getList();
        int offset = allocator(sizeInWords, tempList);
        delete[] static_cast<uint16_t*>(tempList);
        if(offset>=numBlocks){
            return nullptr;
        }
        if(offset == -1)
            return nullptr;
        int it=0;
        int temp=0;
        while(temp<offset){
            temp+=v_blocks[it].sizeInWords;
            it++;
        }
        Block tempBlock(static_cast<void*>(static_cast<char*>(m_start)+offset*wordSize), true, sizeInWords);
        v_blocks.insert(v_blocks.begin()+it, tempBlock);
        it++;
        int newSize = v_blocks[it].sizeInWords-sizeInWords;
        if(newSize==0){
            v_blocks.erase(v_blocks.begin()+it);
        }
        else{
            v_blocks[it].setSize(newSize);
            v_blocks[it].setAddress(static_cast<void*>(static_cast<char*>(m_start)+offset*wordSize+sizeInWords*wordSize));
        }
        return (void*)(static_cast<char*>(m_start)+offset*wordSize);
    }
    void MemoryManager::free(void *address){
        int it = 0;
        while(v_blocks[it].address!=address){
            it++;
        }
        if(it+1==v_blocks.size()||it==0){
            if(it==0){
                if(v_blocks[it+1].allocated)
                    v_blocks[it].allocated = false;
                else{
                    v_blocks[it].sizeInWords += v_blocks[it+1].sizeInWords;
                    v_blocks[it].allocated = false;
                    v_blocks.erase(v_blocks.begin()+1);
                }
            }
            else{
                if(v_blocks[it-1].allocated)
                    v_blocks[it].allocated = false;
                else{
                    v_blocks[it-1].sizeInWords += v_blocks[it].sizeInWords;
                    v_blocks.erase(v_blocks.begin()+it);
                }                    
            }
            return;
        }
        if(v_blocks[it-1].allocated){
            if(v_blocks[it+1].allocated)
                v_blocks[it].allocated = false;
            else{
                v_blocks[it].sizeInWords += v_blocks[it+1].sizeInWords;
                v_blocks[it].allocated = false;
                v_blocks.erase(v_blocks.begin()+it+1);
            }
        }
        else
            if(v_blocks[it+1].allocated){
                v_blocks[it-1].sizeInWords += v_blocks[it].sizeInWords;
                v_blocks.erase(v_blocks.begin()+it);
            }
            else{
                v_blocks[it-1].sizeInWords += v_blocks[it].sizeInWords;
                v_blocks[it-1].sizeInWords += v_blocks[it+1].sizeInWords;
                v_blocks.erase(v_blocks.begin()+it+1);
                v_blocks.erase(v_blocks.begin()+it);
            }
    }
    void MemoryManager::setAllocator(std::function<int(int,void *)> allocator){
        this->allocator=allocator;
    }
    int MemoryManager::dumpMemoryMap(char *filename){
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if(fd == -1)
            return -1;
        void* list = getList();
        string temp = "";
        uint16_t* holeArray = static_cast<uint16_t*>(list);
        int numHoles=holeArray[0];
        for(int i=1; i<numHoles*2+1; i+=2){
            temp=temp+" - ["+to_string(holeArray[i])+", "+to_string(holeArray[i+1])+"]";
        }
        delete[] holeArray;
        string val = temp.substr(3);
        if(write(fd, val.c_str(), val.size()) == -1){
            close(fd);
            return -1;
        }
        close(fd);
        return 0;
    }
    void *MemoryManager::getList(){
        int numHoles=0;
        for(Block b:v_blocks)
            if(!b.allocated)
                numHoles++;
        uint16_t* list = new uint16_t[1+numHoles*2];
        int count = 0;
        list[count]=numHoles;
        count++;
        int offSet=0;
        for(Block b:v_blocks){
            if(!b.allocated){
                list[count]=offSet;
                count++;
                list[count]=b.sizeInWords;
                count++;
            }
            offSet+=b.sizeInWords;
        }
        return static_cast<void*>(list);
    }
    void *MemoryManager::getBitmap(){
        int* bitmap = new int[numBlocks];
        int count=0;
        for(Block b: v_blocks){
            if(b.allocated)
                for(int i = 0; i < b.sizeInWords; i++){
                    bitmap[count]=1;
                    count++;
                }
            else
                for(int i = 0; i < b.sizeInWords; i++){
                    bitmap[count]=0;
                    count++;
                }
        }
        uint16_t bitmapSize = numBlocks/8;
        if(numBlocks%8!=0)
            bitmapSize++;
        uint8_t* tempBitmap = new uint8_t[bitmapSize+2];
        uint8_t temp1 = uint8_t(bitmapSize>>8);
        uint8_t temp0 = uint8_t((bitmapSize<<8)>>8);
        tempBitmap[0] = temp0;
        tempBitmap[1] = temp1;
        for(int i = 2; i < bitmapSize+2; i++){
            uint8_t temp = 0;
            int index = (i-2)*8;
            for(int j = 0; j < 8; j++){
                if(j + index == numBlocks)
                    break;
                temp += bitmap[j + index] << j;
            }
            tempBitmap[i] = temp;
        }
        delete[] bitmap;
        return static_cast<void*>(tempBitmap);
        
    }
    unsigned MemoryManager::getWordSize(){
        return wordSize;
    }
    void *MemoryManager::getMemoryStart(){
        return m_start;
    }
    unsigned MemoryManager::getMemoryLimit(){
        return numBlocks*wordSize;
    }

int bestFit(int sizeInWords, void *list){
    bool found = false;
    int wordOffset = 0;
    int bestDiff = INT_MAX;
    int bestWordOffset = 0;
    uint16_t* holeArray = static_cast<uint16_t*>(list);
    int numHoles=holeArray[0];
    for(int i=1; i<numHoles*2+1; i+=2){
        wordOffset=holeArray[i];
        int diff=holeArray[i+1]-sizeInWords;
        if(diff>=0){
            found = true;
            if(bestDiff>diff){
                bestDiff=diff;
                bestWordOffset=wordOffset;
            }
        }
    }
    if(found)
        return bestWordOffset;
    return -1;
}

int worstFit(int sizeInWords, void *list){
    bool found = false;
    int wordOffset = 0;
    int worstDiff = INT_MIN;
    int worstWordOffset = 0;
    uint16_t* holeArray = static_cast<uint16_t*>(list);
    int numHoles=holeArray[0];
    for(int i=1; i<numHoles*2+1; i+=2){
        wordOffset=holeArray[i];
        int diff=holeArray[i+1]-sizeInWords;
        if(diff>=0){
            found = true;
            if(worstDiff<diff){
                worstDiff=diff;
                worstWordOffset=wordOffset;
            }
        }
    }
    if(found)
        return worstWordOffset;
    return -1;
}
