#include "stdafx.h"
#include "file_instance.h"

FileInstance::FileInstance(const std::wstring& file_path)
	: file_(file_path)
{

}

FileInstance::~FileInstance()
{

}

void FileInstance::StartCapture(FileChangedCallback cb)
{
	file_changed_callback_ = cb;
	file_capture_thread_.reset(new std::thread(&FileInstance::CaptureFileThread, this));
	file_capture_thread_->detach();
}

bool FileInstance::StopCapture()
{
	if (find_handle_)
	{
		FindCloseChangeNotification(find_handle_);
		find_handle_ = nullptr;
	}
	stop_capture_ = true;
	return true;
}

bool FileInstance::ClearFile()
{
	bool	result = false;
	HANDLE  file_handle = NULL;

	file_handle = CreateFile(file_.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file_handle)
	{
		result = true;
	}

	CloseHandle(file_handle);
	file_handle = NULL;

	return result;
}

void FileInstance::CaptureFileThread()
{
	std::wstring file_path = file_.substr(0, file_.rfind(L"\\"));
	find_handle_ = FindFirstChangeNotification(file_.substr(0, file_.rfind(L"\\")).c_str(), FALSE, FILE_NOTIFY_CHANGE_SIZE);
	if (!find_handle_)
	{
		QLOG_ERR(L"Failed to watch directory: {0}") << file_path.c_str();
		return;
	}

	while (true)
	{
		if (!file_first_load_ && !stop_capture_)
		{
			// �ȴ��ļ���С������¼�
			DWORD wait = WaitForSingleObject(find_handle_, INFINITE);
			if (wait == WAIT_OBJECT_0)
			{
				if (!FindNextChangeNotification(find_handle_))
				{
					break;
				}
			}
			else
			{
				// ���ز��� WAIT_OBJECT_0 ���߾���Ѿ���Ч
				break;
			}
		}

		if (stop_capture_)
		{
			break;
		}

		DWORD trunk_file_size = 0;
		curr_file_size_ = GetFileSizeBytes();

		// ��ǰ�ļ�Ϊ�գ�����Ƿ��һ�ζ�ȡ�ļ�Ϊ��󣬵ȴ��ļ����
		if (curr_file_size_ == 0)
		{
			file_first_load_ = false;
			continue;
		}

		// ���Ч�ʣ���Ϊ��ص���Ŀ¼������Ŀ¼�������ļ��޸Ķ��ᱻ���
		// �����ǰ����ļ���Сû�䣬��������
		if (curr_file_size_ == last_file_size_)
		{
			continue;
		}

		// ������ļ�ʧ�ܿ����ļ���ɾ�����߱���������ռ��
		// ����ֱ�� continue �ȴ���һ���ļ������Ϣ���־��п������´����µ���־�ļ���
		HANDLE file_handle = CreateFile(file_.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, 
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (INVALID_HANDLE_VALUE == file_handle)
		{
			continue;
		}

		// �ļ��������ȡ��������ݣ��ļ������С�������½�ȡ�ļ�
		if (curr_file_size_ > last_file_size_)
		{
			trunk_file_size = curr_file_size_ - last_file_size_;
			SetFilePointer(file_handle, last_file_size_, NULL, FILE_BEGIN);
		}
		else
		{
			trunk_file_size = curr_file_size_;
			SetFilePointer(file_handle, 0, NULL, FILE_BEGIN);
		}

		// �����
		// ��Ҫ��ȡ�����ݹ���ʱ����ֻ��ȡ�Լ��趨ֵ�����ݴ�С

		// ����Ҫ��ȡ�����ݴ�С�����ڴ�
		std::shared_ptr<TCHAR> buffer(new TCHAR[trunk_file_size + 1]);
		ZeroMemory(buffer.get(), trunk_file_size + 1);

		// ��ʼ��ȡָ����С������
		DWORD  read_bytes = 0;
		if (ReadFile(file_handle, buffer.get(), trunk_file_size, &read_bytes, NULL))
		{
			QLOG_APP(L"File {0} changed, current file size = {1}, last file size = {2}") 
				<< file_.c_str() << curr_file_size_ << last_file_size_;

			bool append = curr_file_size_ >= last_file_size_;

			// �ص����ϲ� UI
			Post2UI(ToWeakCallback([this, buffer, append]() {
				file_changed_callback_(file_, (PCHAR)buffer.get(), append);
			}));

			// ��¼����ȡ�ļ���λ��
			last_file_size_ = curr_file_size_;
		}

		// ��ȡһ���ļ����Ƿ��һ�ζ�ȡ��״̬��Ϊ��
		if (file_first_load_)
		{
			file_first_load_ = false;
		}

		if (file_handle)
		{
			CloseHandle(file_handle);
			file_handle = NULL;
		}
	}
}

DWORD FileInstance::GetFileSizeBytes()
{
	HANDLE  file_handle = NULL;
	DWORD   file_size = 0;

	file_handle = CreateFile(file_.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, 
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file_handle != INVALID_HANDLE_VALUE)
	{
		file_size = GetFileSize(file_handle, NULL);
		CloseHandle(file_handle);
		file_handle = NULL;
	}

	return file_size;
}

