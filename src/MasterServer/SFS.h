#ifndef SSFS_SFS_H
#define SSFS_SFS_H


#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "json/json.h"

struct BlockInfo
{
    std::string blockhash;
    int64_t blocksize;

    BlockInfo(const std::string& blockhash, int64_t size = 0)
            :blockhash(blockhash), blocksize(size)
    {
        ;
    }
};
typedef std::shared_ptr<BlockInfo> BlockInfoPtr;

struct FileInfo
{
    std::string filehash;
    int64_t filesize;
    std::vector<std::string> blocks;

    FileInfo(const std::string& filehash, int64_t filesize)
            :filehash(filehash), filesize(filesize)
    {
        ;
    }

    void addBlock(const std::string& blockhash)
    {
        blocks.push_back(blockhash);
    }
};
typedef std::shared_ptr<FileInfo> FileInfoPtr;

class ChunkInfo
{
private:
    int status_;
    std::string ip_;
    std::uint16_t port_;
    std::set<std::string> blocks_;

public:
    ChunkInfo(){ status_ = 0; }
    ChunkInfo(const std::string& ip, uint16_t port, int status = 1)
            :ip_(ip), port_(port), status_(status)
    {
        ;
    }

    int getStatus() const
    {
        return status_;
    }

    void setStatus(int status)
    {
        status_ = status;
    }

    std::string getIp() const
    {
        return ip_;
    }

    uint16_t getPort() const
    {
        return port_;
    }

    void addBlock(const std::string& blockhash)
    {
        blocks_.insert(blockhash);
    }

    bool isExistBlock(const std::string& blockhash) const
    {
        return blocks_.find(blockhash) != blocks_.end();
    }
};
typedef std::shared_ptr<ChunkInfo> ChunkInfoPtr;
typedef std::weak_ptr<ChunkInfo> WeakChunkInfoPtr;
typedef std::pair<std::string, uint16_t> CHUNK;

class SFS
{
private:
    std::map<std::string, std::string> name2hash_;
    std::map<std::string, FileInfoPtr> hash2file_;
    std::map<std::string, BlockInfoPtr> hash2block_;
    std::map<CHUNK, ChunkInfoPtr> chunkservers_;
    std::list<WeakChunkInfoPtr> chunkList_;

public:
    static SFS *getInstance()
    {
        static SFS instance;
        return &instance;
    }

    bool isExistFileName(const std::string &filename)
    {
        return name2hash_.find(filename) != name2hash_.end();
    }

    bool isExistFileHash(const std::string &filehash)
    {
        return hash2file_.find(filehash) != hash2file_.end();
    }

    bool isExistBlockHash(const std::string &blockhash, bool check = false)
    {
        if(check == false)
            return hash2block_.find(blockhash) != hash2block_.end();
        for(auto& chunk : chunkservers_)
            if(chunk.second->isExistBlock(blockhash))
                return true;
        return false;
    }

    bool isExistChunk(const std::string& ip, uint16_t port)
    {
        return chunkservers_.find(std::make_pair(ip, port)) != chunkservers_.end();
    }

    //获取文件列表 文件名 文件大小 文件hash
    std::string getFileList()
    {
        Json::FastWriter fw;
        Json::Value fileList;
        fileList.resize(0);
        for(auto &f : name2hash_)
        {
            Json::Value file;
            file["filename"] = f.first;
            file["filesize"] = Json::Int64 (hash2file_[f.second]->filesize);
            fileList.append(file);
        }
        return fw.write(fileList);
    }
    //获取文件的详细信息 文件名 文件大小 文件hash 文件各个块
    std::string getFileInfo(const std::string& filename)
    {
        Json::FastWriter fw;
        Json::Value fileinfo;
        fileinfo["filename"] = filename;
        auto& fileinfoptr = hash2file_[ name2hash_[filename] ];
        fileinfo["filesize"] = Json::Int64 (fileinfoptr->filesize);
        fileinfo["filehash"] = fileinfoptr->filehash;
        fileinfo["blocks"].resize(0);
        for(auto &b : fileinfoptr->blocks)
        {
            Json::Value block;
            auto& bl = hash2block_[b];
            block["blockhash"] = bl->blockhash;
            block["blocksize"] = Json::Int64(bl->blocksize);
            block["chunklist"].resize(0);
            for(auto& chunk : chunkservers_)
            {
                if(chunk.second->getStatus() && chunk.second->isExistBlock(bl->blockhash))
                {
                    Json::Value ck;
                    ck["chunkip"] = chunk.first.first;
                    ck["chunkport"] = chunk.first.second;
                    block["chunklist"].append(ck);
                }
            }
            fileinfo["blocks"].append(block);
        }
        return fw.write(fileinfo);
    }

    //get chunk list
    std::string getChunkList(int status = 1)
    {
        Json::Value chunklist;
        Json::FastWriter fw;
        chunklist.resize(0);
        for(auto &chunk : chunkservers_)
        {
            if(chunk.second->getStatus() == status)
            {
                Json::Value ck;
                ck["ip"] = chunk.first.first;
                ck["port"] = chunk.first.second;
                chunklist.append(ck);
            }
        }
        return fw.write(chunklist);
    }

    FileInfoPtr getFileInfoPtr(const std::string& filename)
    {
        return hash2file_[ name2hash_[filename] ];
    }

    FileInfoPtr addFile(const std::string& filename, int64_t filesize, const std::string& filehash)
    {
        if(!isExistFileHash(filehash))
        {
            hash2file_[filehash].reset(new FileInfo(filehash, filesize));
        }

        name2hash_[filename] = filehash;
        return hash2file_[filehash];
    }

    bool rmFile(const std::string& filename)
    {
        if(!isExistFileName(filename))
            return false;
        name2hash_.erase(filename);
        return true;
    }

    bool mvFile(const std::string& oldFilename, const std::string& newFilename)
    {
        if(!isExistFileName(oldFilename))
            return false;
        if(isExistFileName(newFilename))
            return false;
        name2hash_[newFilename] = name2hash_[oldFilename];
        name2hash_.erase(oldFilename);
        return true;
    }

    //chunk add block
    void addBlock(const std::string& blockhash, int64_t blocksize)
    {
        if(!isExistBlockHash(blockhash))
            hash2block_[blockhash] = std::make_shared<BlockInfo>(blockhash, blocksize);
    }

    ChunkInfoPtr getChunkInfoPtr(const std::string& ip, uint16_t port)
    {
        if(!isExistChunk(ip, port))
        {
            ChunkInfoPtr p = std::make_shared<ChunkInfo>(ip, port);
            chunkservers_[ std::make_pair(ip, port) ] = p;
            chunkList_.push_back(WeakChunkInfoPtr(p));
        }
        return chunkservers_[ std::make_pair(ip, port) ];
    }

    ChunkInfoPtr getNextChunkInfoPtr()
    {
        while(1)
        {
            if(chunkList_.size() <= 0) break;
            WeakChunkInfoPtr p = (chunkList_.front());
            chunkList_.pop_front();
            ChunkInfoPtr ckptr = p.lock();
            if(ckptr && ckptr->getStatus())
            {
                chunkList_.push_back(p);
                return ckptr;
            }
        }
        return ChunkInfoPtr(nullptr);
    }

private:
    SFS(){}
};
#define FS SFS::getInstance()


#endif //SSFS_SFS_H
