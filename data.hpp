#ifndef __MY_DATA__
#define __MY_DATA__
#include "util.hpp"

namespace cloud
{
	typedef struct _FileInfo
	{
		std::string filename;	//文件名
		std::string url_path;	//下载链接路径
		std::string real_path;	//实际存储路径
		size_t file_size;		//文件大小
		time_t back_time;		//备份时间
		bool pack_flag;			//压缩标志
		std::string pack_path;	//压缩包路径名
	}FileInfo;

	class DataManager
	{
		private:
			std::string _back_dir="./backup_dir/";			//备份文件的实际存储路径
			std::string _pack_dir="./pack_dir/";			//压缩包存储路径
			std::string _download_prefix = "/download/";	//下载链接的前缀路径 http://192.168.56.105/download/a.txt
			std::string _pack_subfix = ".zip";				//压缩包的后缀名
			std::string _back_info_file = "./backup.dat";	//存储备份信息的文件
			//用url作为key，是因为当前端下载文件时，就会发送过来url
			std::unordered_map<std::string,FileInfo> _back_info;	//<url,fileinfo> 备份信息
		public:
			DataManager()
			{
				FileUtil(_back_dir).CreateDirectory();
				FileUtil(_pack_dir).CreateDirectory();
				if(FileUtil(_back_info_file).Exists())
				{
					InitLoad();
				}
			}
			//当模块运行起来后，第一时间将历史信息读取出来存储到hash表中
			bool InitLoad()
			{
				//1.读取文件中的历史备份信息	
				std::string body;
				bool ret = FileUtil(_back_info_file).Read(&body);
				if(ret == false)
				{
					std::cout << "initload history failed!\n";
					return false;
				}
				//2.对读取的信息进行反序列化
				Json::Value infos;
				ret = JsonUtil::UnSerialize(body,&infos);
				if(ret == false)
				{
					std::cout << "initload parse history failed!\n";
					return false;
				}
				//3.将反序列化得到的数据存储到hash表中
				int sz = infos.size();
				for(int i=0;i<sz;i++)
				{
					FileInfo info;
					info.filename = infos[i]["filename"].asString();
					info.url_path = infos[i]["url_path"].asString();
					info.real_path = infos[i]["real_path"].asString();
					info.file_size = infos[i]["file_size"].asInt64();
					info.back_time = infos[i]["back_time"].asInt64();
					info.pack_flag = infos[i]["pack_flag"].asBool();
					info.pack_path = infos[i]["pack_path"].asString();
					_back_info[info.url_path] = info;
				}
				return true;
			}

			bool Storage()
			{
				Json::Value infos;
				std::vector<FileInfo>arry;
				this->SelectAll(&arry);
				
				for(auto &a:arry)
				{
					Json::Value info;
					info["filename"]=a.filename;
					info["url_path"]=a.url_path;
					info["real_path"]=a.real_path;
					info["file_size"]=(Json::UInt64)a.file_size;
					info["back_time"]=(Json::UInt64)a.back_time;
					info["back_flag"]=a.pack_flag;
					info["pack_path"]=a.pack_path;
					infos.append(info);
				}
				std::string body;
				JsonUtil::Serialize(infos,&body);
				FileUtil(_back_info_file).Write(body);
				return true;
			}
			//数据的增删改查;
			//	增：输入一个文件名，在接口中获取各项信息，生成压缩包名称，填充结构体，压入hash
			bool Insert(const std::string &pathname)
			{
				if(FileUtil(pathname).Exists()==false)
				{
					std::cout << "insert file is not exists!\n";
					return false;
				}
				// pathname = ./backup_dir/a.txt
				FileInfo info;
				info.filename = FileUtil(pathname).Name();		//a.txt
				info.url_path = _download_prefix + info.filename;	// /download/a.txt	下载链接的资源路径
				info.real_path = pathname;		//实际存储路径	./backup_dir/a.txt
				info.file_size = FileUtil(pathname).Size();		//文件大小
				info.back_time = FileUtil(pathname).MTime();	//最后一次修改时间就是备份时间
				info.pack_flag = false;		//刚上传的文件都是非压缩状态
				info.pack_path = _pack_dir + info.filename + _pack_subfix;	//压缩包路径名	./pack_dir/a.txt.zip
				_back_info[info.url_path] = info;	//以url_path为key,info为value添加到map中;
				Storage();
				return true;
			}
			//	改：当文件压缩存储了，要修改压缩标志，
			//		当下载时，解压后也要修改压缩标志
			bool UpdateStatus(const std::string &pathname,bool status)
			{
				std::string url_path = _download_prefix +  FileUtil(pathname).Name();
				auto it = _back_info.find(url_path);
				if(it == _back_info.end())
				{
					std::cout << "file info is not exists!\n";
					return false;
				}
				it->second.pack_flag = status;
				Storage();
				return true;
			}
			//	查：查询所有备份信息（前端展示界面需要-文件名，下载链接，大小，备份时间）
			//		查询单个文件信息（文件下载-获取文件实际的备份路径）
			bool SelectAll(std::vector<FileInfo> *infos)
			{
				for(auto it = _back_info.begin();it != _back_info.end();++it)
				{
					infos->push_back(it->second);
				}
				return true;
			}
			//通过key值也就是utl_path获取备份信息
			bool SelectOne(const std::string &url_path,FileInfo *info)
			{
				auto it = _back_info.find(url_path);
				if(it == _back_info.end())
				{
					std::cout << "file info is not exists!\n";
					return false;
				}
				*info = it->second;
				return true;
			}
			bool SelectOneByRealPath(const std::string &realpath,FileInfo *info)
			{
				for(auto it = _back_info.begin();it != _back_info.end();++it)
				{
					if(it->second.real_path == realpath)
					{
						*info = it->second;
						return true;
					}
				}
				return false;
			}
			//	删：基本备份的文件都不删除（防备功能扩展）
			bool DeleteOne(const std::string &url_path)
			{	
				auto it = _back_info.find(url_path);
				if(it == _back_info.end())
				{
					std::cout << "file info is not exists!\n";
					return false;
				}
				_back_info.erase(it);
				Storage();
				return true;
			}
	};
}

#endif
