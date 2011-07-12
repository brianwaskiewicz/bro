// See the file "COPYING" in the main distribution directory for copyright.

#include "util.h"
#include "LogWriter.h"

LogWriter::LogWriter()
	{
	buf = 0;
	buf_len = 1024;
	buffering = true;
	this->start();
	}

LogWriter::~LogWriter()
	{
	if ( buf )
		free(buf);

	for(int i = 0; i < num_fields; ++i)
		delete fields[i];

	delete [] fields;
	}

bool LogWriter::Init(string arg_path, int arg_num_fields,
		     const LogField* const * arg_fields)
	{
	path = arg_path;
	num_fields = arg_num_fields;
	fields = arg_fields;

	putMessage(new InitMessage(arg_path, arg_num_fields, arg_fields));
	return true;
	}

bool LogWriter::Write(int arg_num_fields, LogVal** vals)
	{
	// Double-check that the arguments match. If we get this from remote,
	// something might be mixed up.
	if ( num_fields != arg_num_fields )
		{
		DBG_LOG(DBG_LOGGING, "Number of fields don't match in LogWriter::Write() (%d vs. %d)",
			arg_num_fields, num_fields);

		return false;
		}

	for ( int i = 0; i < num_fields; ++i )
		{
		if ( vals[i]->type != fields[i]->type )
			{
			DBG_LOG(DBG_LOGGING, "Field type doesn't match in LogWriter::Write() (%d vs. %d)",
				vals[i]->type, fields[i]->type);
			return false;
			}
		}

	putMessage(new WriteMessage(num_fields, fields, vals));

	return true;
	}

bool LogWriter::SetBuf(bool enabled)
	{
	putMessage(new BufferMessage(enabled));
	return true;
	}

bool LogWriter::Rotate(string rotated_path, string postprocessor, double open,
		       double close, bool terminating)
	{
	putMessage(new RotateMessage(rotated_path, postprocessor, open, close, terminating));
	if(terminating)
		{
		Finish();
		}
	return true;
	}

bool LogWriter::Flush()
	{
	putMessage(new FlushMessage());
	return true;
	}

void LogWriter::Finish()
	{
	putMessage(new FinishMessage());
	}

const char* LogWriter::Fmt(const char* format, ...)
	{
	if ( ! buf )
		buf = (char*) malloc(buf_len);

	va_list al;
	va_start(al, format);
	int n = safe_vsnprintf(buf, buf_len, format, al);
	va_end(al);

	if ( (unsigned int) n >= buf_len )
		{ // Not enough room, grow the buffer.
		buf_len = n + 32;
		buf = (char*) realloc(buf, buf_len);

		// Is it portable to restart?
		va_start(al, format);
		n = safe_vsnprintf(buf, buf_len, format, al);
		va_end(al);
		}

	return buf;
	}

void LogWriter::Error(const char *msg)
	{
	log_mgr->Error(this, msg);
	}

void LogWriter::DeleteVals(LogVal** vals)
	{
	for ( int i = 0; i < num_fields; i++ )
		delete vals[i];
	delete[] vals;
	}

bool LogWriter::RunPostProcessor(string fname, string postprocessor,
				 string old_name, double open, double close,
				 bool terminating)
	{
	// This function operates in a way that is backwards-compatible with
	// the old Bro log rotation scheme.

	if ( ! postprocessor.size() )
		return true;

	const char* const fmt = "%y-%m-%d_%H.%M.%S";

	struct tm tm1;
	struct tm tm2;

	time_t tt1 = (time_t)open;
	time_t tt2 = (time_t)close;

	localtime_r(&tt1, &tm1);
	localtime_r(&tt2, &tm2);

	char buf1[128];
	char buf2[128];

	strftime(buf1, sizeof(buf1), fmt, &tm1);
	strftime(buf2, sizeof(buf2), fmt, &tm2);

	string cmd = postprocessor;
	cmd += " " + fname;
	cmd += " " + old_name;
	cmd += " " + string(buf1);
	cmd += " " + string(buf2);
	cmd += " " + string(terminating ? "1" : "0");
	cmd += " &";

	system(cmd.c_str());

	return true;
	}
