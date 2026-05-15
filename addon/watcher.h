#ifndef __watcher_h__
#define __watcher_h__
#include "extender.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <thread>
#include <unordered_map>
#ifdef __linux__
#include "inotify.h"
#endif

class Watcher : public Extender {
public:
	Watcher ();
	~Watcher () override;

private:
	class Observer {
	public:
#ifdef _WIN32
		const long watcherBuffer {
				16380 *
				2 }; // https://qualapps.blogspot.ca/2010/05/understanding-readdirectorychangesw_19.html
		FILE_NOTIFY_INFORMATION* info;
#endif
		Observer ( Watcher* Parent );
		void Start ();

	private:
		struct Actions {
			static constexpr WCHAR_T Added [] = { '1', '\0' };
			static constexpr WCHAR_T Removed [] = { '2', '\0' };
			static constexpr WCHAR_T Changed [] = { '3', '\0' };
			static constexpr WCHAR_T RenamedOld [] = { '4', '\0' };
			static constexpr WCHAR_T RenamedNew [] = { '5', '\0' };
			static constexpr WCHAR_T FolderAdded [] = { '6', '\0' };
			static constexpr WCHAR_T FolderRemoved [] = { '7', '\0' };
			static constexpr WCHAR_T FolderChanged [] = { '8', '\0' };
			static constexpr WCHAR_T FolderRenamedOld [] = { '9', '\0' };
			static constexpr WCHAR_T FolderRenamedNew [] = { '0', '\0' };
			static constexpr WCHAR_T WatcherEntryRemoved [] = { 'a', '\0' };
			static constexpr WCHAR_T WatcherEntryMoved [] = { 'b', '\0' };
			static constexpr WCHAR_T VolumeUnmounted [] = { 'c', '\0' };
			static constexpr WCHAR_T WatcherOverflow [] = { 'd', '\0' };
			static constexpr WCHAR_T WatcherDisconnected [] = { 'e', '\0' };
		};

		Watcher* Parent;
		std::filesystem::path Folder;
		std::wstring File;
		std::vector<std::byte> Buffer;

#ifdef __linux__
		void observing () const;
		void sendMessage ( const Inotify::Notification* Notification ) const;
		static const WCHAR_T* actionToName ( const Inotify::Action& Event );
		static bool hasEvent ( const Inotify::Action& Source,
													 const Inotify::Action& Event );
#elif _WIN32
		void observing ();
		void sendMessage ( DWORD Offset = 0 );
		[[nodiscard]]
		const WCHAR_T* actionToName () const;
#endif
	};

	struct File {
		std::string name;
		uint64_t content;
		uintmax_t size;
		std::filesystem::file_time_type changed;
	};

	class Path {
	public:
		Path& operator= ( const std::wstring& folder );
		operator const std::filesystem::path&() const&;
		operator const std::filesystem::path&() const&& = delete;

	private:
		std::filesystem::path path;
	};

#ifdef __linux__
	Inotify::Inotify Notifier;
#elif _WIN32
	HANDLE FolderID;
#endif
	std::thread* Thread;
	Path watchingFolder;
	bool Active;
	bool Paused;
	std::filesystem::file_time_type lastCheck, previousCheck;
	std::unordered_map<uint64_t, File> files;
	static void startObserver ( Watcher* Parent );
	void stopWatching ();
	bool startWatching ( tVariant* Params );
	bool getChanges ( tVariant* Result );
	std::optional<std::pair<uint64_t, uint64_t>> storeInfo ( auto& entry );
	bool watch ( tVariant* Params );
	bool pause ();
	bool resume ();
	bool checkActivity ();
	void activate ();
	void deactivate ();
};
#endif
