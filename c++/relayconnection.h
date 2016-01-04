#ifndef _RELAY_RELAYCONNECTION_H
#define _RELAY_RELAYCONNECTION_H

#include "preinclude.h"

#include "relayprocess.h"

#include <vector>
#include <chrono>
#include <memory>
#include <functional>

class RelayConnectionProcessor {
private:
	enum ReadState {
		READ_STATE_NEW_MESSAGE,
		READ_STATE_IN_SIZED_MESSAGE,
		READ_STATE_IN_BLOCK_MESSAGE,
		READ_STATE_DISCONNECTED,
	};
	ReadState read_state;
	bool have_received_version_msg;
	std::vector<char> read_buff;

	relay_msg_header current_msg;
	RelayNodeCompressor::DecompressState current_block;
	std::unique_ptr<RelayNodeCompressor::DecompressLocks> current_block_locks;

	std::chrono::steady_clock::time_point current_block_read_start;

	RELAY_DECLARE_CLASS_VARS

protected:
	RelayNodeCompressor compressor;

public:
	RelayConnectionProcessor() : read_state(READ_STATE_NEW_MESSAGE), have_received_version_msg(false),
			 RELAY_DECLARE_CONSTRUCTOR_EXTENDS, compressor(false, true) {}
			// The compressor options above are used by the cilent for "current version" connections

protected:
	virtual const char* handle_peer_version(const std::string& peer_version)=0;
	virtual const char* handle_max_version(const std::string& max_version)=0;
	virtual const char* handle_sponsor(const std::string& sponsor)=0;
	virtual void handle_pong(uint64_t nonce)=0;

	virtual void handle_block(RelayNodeCompressor::DecompressState& block,
			std::chrono::system_clock::time_point& read_end_time,
			std::chrono::steady_clock::time_point& read_end,
			std::chrono::steady_clock::time_point& read_start)=0;
	virtual void handle_transaction(std::shared_ptr<std::vector<unsigned char> >& tx)=0;

	virtual void disconnect(const char* reason)=0;
	virtual void do_send_bytes(const char *buf, size_t nbyte)=0;

	void recv_bytes(char* buf, size_t len);

	void reset_read_state();

private:
	inline size_t fail_msg(const char* reason) {
		disconnect(reason);
		read_state = READ_STATE_DISCONNECTED;
		return 0;
	}

	bool check_message_header();
	bool process_version_message();
	bool process_max_version_message();
	bool process_sponsor_message();
	void start_block_message();
	ssize_t process_block_message(char* this_read_buf, size_t read_buf_len);
	bool process_transaction_message(bool outOfBand);
	bool process_ping_message();
	bool process_pong_message();
};

#endif
