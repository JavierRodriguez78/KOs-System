
#include "console/logger.hpp"

namespace kos {
	namespace console {

		/*
		*@brief Stub implementations for log to journal functions
		*@param msg Message to log
		*/
		void LogToJournal(const char* msg) {}

		/*
		*@brief Stub implementation for log to journal key-value functions
		*@param key Key to log
		*@param value Value to log
		*/
		void LogToJournalKV(const char* key, const char* value) {}

		/*
		*@brief Stub implementation for log to journal status functions
		*@param msg Message to log
		*@param ok Status to log
		*/
		void LogToJournalStatus(const char* msg, bool ok) {}

	} // namespace console
} // namespace kos
