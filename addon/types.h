#pragma once
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stop_token>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace types {
template <typename Value> class SafeValue {
public:
	SafeValue ( Value value = {} ) : value ( std::move ( value ) ) {
	}

	SafeValue ( const SafeValue& ) = delete;
	SafeValue ( SafeValue&& ) = delete;
	SafeValue& operator= ( const SafeValue& ) = delete;
	SafeValue& operator= ( SafeValue&& ) = delete;

	SafeValue& operator= ( Value newValue ) {
		bool changed = false;
		{
			std::lock_guard<std::mutex> lock ( mutex );
			if constexpr ( requires ( const Value& current, const Value& updated ) {
											 current != updated;
										 } ) {
				changed = value != newValue;
			} else {
				changed = true;
			}
			value = std::move ( newValue );
		}
		if ( changed ) {
			condition.notify_all ();
		}
		return *this;
	}

	[[nodiscard]] Value get () const {
		std::lock_guard<std::mutex> lock ( mutex );
		return value;
	}

	bool operator== ( const Value& other ) const {
		std::lock_guard<std::mutex> lock ( mutex );
		return value == other;
	}

	operator Value () const {
		return get ();
	}

	void wait ( const Value RequiredValue,
							const std::stop_token& stop = std::stop_token {} ) {
		std::stop_callback callback { stop, [ this ] { condition.notify_all (); } };
		std::unique_lock<std::mutex> lock ( mutex );
		condition.wait ( lock, [ this, &RequiredValue, &stop ] {
			return value == RequiredValue || stop.stop_requested ();
		} );
	}

	template <typename Predicate>
	[[nodiscard]] bool
	waitFor ( const unsigned int seconds, Predicate predicate,
						const std::stop_token& stop = std::stop_token {} ) {
		std::stop_callback callback { stop, [ this ] { condition.notify_all (); } };
		std::unique_lock<std::mutex> lock ( mutex );
		condition.wait_for ( lock, std::chrono::seconds ( seconds ),
												 [ this, &predicate, &stop ] {
													 return predicate ( value ) || stop.stop_requested ();
												 } );
		return predicate ( value );
	}

private:
	Value value;
	mutable std::mutex mutex;
	std::condition_variable condition;
};

template <typename Key, typename Value> class SafeMap {
public:
	SafeMap () = default;
	~SafeMap () = default;
	SafeMap ( SafeMap&& ) = delete;
	SafeMap ( const SafeMap& ) = delete;
	SafeMap& operator= ( SafeMap&& ) = delete;
	SafeMap& operator= ( const SafeMap& ) = delete;

	void insert ( const Key& key, const Value& value ) {
		std::unique_lock<std::shared_mutex> lock ( mutex );
		map [ key ] = std::make_shared<Value> ( value );
	}

	void remove ( const Key& key ) {
		std::unique_lock<std::shared_mutex> lock ( mutex );
		map.erase ( key );
	}

	std::shared_ptr<const Value> get ( const Key& key ) const {
		std::shared_lock<std::shared_mutex> lock ( mutex );
		const auto found = map.find ( key );
		if ( found == map.end () ) {
			return nullptr;
		}
		return found->second;
	}

	bool has ( const Key& key ) const {
		std::shared_lock<std::shared_mutex> lock ( mutex );
		return map.find ( key ) != map.end ();
	}

	void clear () {
		std::unique_lock<std::shared_mutex> lock ( mutex );
		map.clear ();
	}

	std::shared_ptr<const std::unordered_map<Key, std::shared_ptr<Value>>>
	snapshot () const {
		std::shared_lock<std::shared_mutex> lock ( mutex );
		return std::make_shared<
				const std::unordered_map<Key, std::shared_ptr<Value>>> ( map );
	}

	void modify ( const Key& key,
								const std::function<void ( Value& )>& modifier ) {
		std::unique_lock<std::shared_mutex> lock ( mutex );
		auto found = map.find ( key );
		if ( found != map.end () ) {
			auto newValue = std::make_shared<Value> ( *found->second );
			modifier ( *newValue );
			found->second = std::move ( newValue );
		}
	}

private:
	mutable std::shared_mutex mutex;
	std::unordered_map<Key, std::shared_ptr<Value>> map;
};
}
