#ifndef MSGCODE_H
#define MSGCODE_H

#include <stdint.h>

//定义报文头部类型和长度
typedef int32_t msgh_t;
const static int HLEN = sizeof(msgh_t);
const static msgh_t MAXHLEN = 64*1024*1024 + 1024;
const static msgh_t MINHLEN = 0;

//定义报文标志类型和长度
typedef int16_t msgflag_t;
const static int FLEN = sizeof(msgflag_t);
const static msgflag_t FLAG = 0x88;

//定义报文消息类型和长度
typedef int16_t msgtype_t;
const static int MSGTYPELEN = sizeof(msgtype_t);

//服务认证
const static msgtype_t AUTH_REQ = 11;
const static msgtype_t AUTH_RES = 12;

//获取配置信息
//const static msgtype_t CONFIG_REQ = 13;
//const static msgtype_t CONFIG_RES = 14;

//获取chunklist
const static msgtype_t CHUNKLIST_REQ = 15;
const static msgtype_t CHUNKLIST_RES = 16;

//获取文件列表
const static msgtype_t FILELIST_REQ = 21;
const static msgtype_t FILELIST_RES = 22;

//获取文件详细信息
const static msgtype_t FILEINFO_REQ = 23;
const static msgtype_t FILEINFO_RES = 24;

//client <--> master
const static msgtype_t UPFILE_REQ = 25;
const static msgtype_t UPFILE_RES = 26;

const static msgtype_t UPFILE_BLOCK_REQ = 27;
const static msgtype_t UPFILE_BLOCK_RES = 28;


//文件更新
//上传完成 新建文件操作
const static msgtype_t ADDFILE_REQ = 31;
const static msgtype_t ADDFILE_RES = 32;

const static msgtype_t RMFILE_REQ = 33;
const static msgtype_t RMFILE_RES = 34;

const static msgtype_t MVFILE_REQ = 35;
const static msgtype_t MVFILE_RES = 36;


//chunk <--> master 新增文件块
const static msgtype_t ADDBLOCK_REQ = 37;
const static msgtype_t ADDBLOCK_RES = 38;

//chunk --> master 心跳包
const static msgtype_t KEEPALIVE_CK = 666;


//client <--> chunk upblock
const static msgtype_t UPBLOCK_REQ = 101;
const static msgtype_t UPBLOCK_RES = 102;

const static msgtype_t UPBLOCK_DATA = 103;
const static msgtype_t UPBLOCK_OVER = 104;
const static msgtype_t UPBLOCK_STOP = 105;

//client <--> chunk downblock
const static msgtype_t DOWNBLOCK_REQ = 106;
const static msgtype_t DOWNBLOCK_RES = 107;

const static msgtype_t DOWNBLOCK_DATA = 108;
const static msgtype_t DOWNBLOCK_OVER = 109;
const static msgtype_t DOWNBLOCK_STOP = 110;


//chunk up block to new chunk
const static msgtype_t BACKUP_BLOCK_REQ = 201;
const static msgtype_t BACKUP_BLOCK_RES = 202;

#endif // MSGCODE_H
