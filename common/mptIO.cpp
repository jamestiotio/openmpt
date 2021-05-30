/*
 * mptIO.cpp
 * ---------
 * Purpose: Basic functions for reading/writing binary and endian safe data to/from files/streams.
 * Notes  : Some useful functions for reading and writing are still missing.
 * Authors: Joern Heusipp
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"

#include "mptIO.h"

#include <ios>
#include <istream>
#include <ostream>
#include <sstream>


OPENMPT_NAMESPACE_BEGIN


namespace mpt {

namespace IO {


//static_assert(sizeof(std::streamoff) == 8); // Assert 64bit file support.
bool IsValid(std::ostream & f) { return !f.fail(); }
bool IsValid(std::istream & f) { return !f.fail(); }
bool IsValid(std::iostream & f) { return !f.fail(); }
bool IsReadSeekable(std::istream & f)
{
	f.clear();
	std::streampos oldpos = f.tellg();
	if(f.fail() || oldpos == std::streampos(-1))
	{
		f.clear();
		return false;
	}
	f.seekg(0, std::ios::beg);
	if(f.fail())
	{
		f.clear();
		f.seekg(oldpos);
		f.clear();
		return false;
	}
	f.seekg(0, std::ios::end);
	if(f.fail())
	{
		f.clear();
		f.seekg(oldpos);
		f.clear();
		return false;
	}
	std::streampos length = f.tellg();
	if(f.fail() || length == std::streampos(-1))
	{
		f.clear();
		f.seekg(oldpos);
		f.clear();
		return false;
	}
	f.seekg(oldpos);
	f.clear();
	return true;
}
bool IsWriteSeekable(std::ostream & f)
{
	f.clear();
	std::streampos oldpos = f.tellp();
	if(f.fail() || oldpos == std::streampos(-1))
	{
		f.clear();
		return false;
	}
	f.seekp(0, std::ios::beg);
	if(f.fail())
	{
		f.clear();
		f.seekp(oldpos);
		f.clear();
		return false;
	}
	f.seekp(0, std::ios::end);
	if(f.fail())
	{
		f.clear();
		f.seekp(oldpos);
		f.clear();
		return false;
	}
	std::streampos length = f.tellp();
	if(f.fail() || length == std::streampos(-1))
	{
		f.clear();
		f.seekp(oldpos);
		f.clear();
		return false;
	}
	f.seekp(oldpos);
	f.clear();
	return true;
}
IO::Offset TellRead(std::istream & f)
{
	return f.tellg();
}
IO::Offset TellWrite(std::ostream & f)
{
	return f.tellp();
}
bool SeekBegin(std::ostream & f)
{
	f.seekp(0); return !f.fail();
}
bool SeekBegin(std::istream & f)
{
	f.seekg(0); return !f.fail();
}
bool SeekBegin(std::iostream & f)
{
	f.seekg(0); f.seekp(0); return !f.fail();
}
bool SeekEnd(std::ostream & f)
{
	f.seekp(0, std::ios::end); return !f.fail();
}
bool SeekEnd(std::istream & f)
{
	f.seekg(0, std::ios::end); return !f.fail();
}
bool SeekEnd(std::iostream & f)
{
	f.seekg(0, std::ios::end); f.seekp(0, std::ios::end); return !f.fail();
}
bool SeekAbsolute(std::ostream & f, IO::Offset pos)
{
	if(!mpt::in_range<std::streamoff>(pos)) { return false; }
	f.seekp(static_cast<std::streamoff>(pos), std::ios::beg); return !f.fail();
}
bool SeekAbsolute(std::istream & f, IO::Offset pos)
{
	if(!mpt::in_range<std::streamoff>(pos)) { return false; }
	f.seekg(static_cast<std::streamoff>(pos), std::ios::beg); return !f.fail();
}
bool SeekAbsolute(std::iostream & f, IO::Offset pos)
{
	if(!mpt::in_range<std::streamoff>(pos)) { return false; }
	f.seekg(static_cast<std::streamoff>(pos), std::ios::beg); f.seekp(static_cast<std::streamoff>(pos), std::ios::beg); return !f.fail();
}
bool SeekRelative(std::ostream & f, IO::Offset off)
{
	if(!mpt::in_range<std::streamoff>(off)) { return false; }
	f.seekp(static_cast<std::streamoff>(off), std::ios::cur); return !f.fail();
}
bool SeekRelative(std::istream & f, IO::Offset off)
{
	if(!mpt::in_range<std::streamoff>(off)) { return false; }
	f.seekg(static_cast<std::streamoff>(off), std::ios::cur); return !f.fail();
}
bool SeekRelative(std::iostream & f, IO::Offset off)
{
	if(!mpt::in_range<std::streamoff>(off)) { return false; }
	f.seekg(static_cast<std::streamoff>(off), std::ios::cur); f.seekp(static_cast<std::streamoff>(off), std::ios::cur); return !f.fail();
}
mpt::byte_span ReadRawImpl(std::istream & f, mpt::byte_span data) { f.read(mpt::byte_cast<char *>(data.data()), data.size()); return data.first(mpt::saturate_cast<std::size_t>(f.gcount())); }
bool WriteRawImpl(std::ostream & f, mpt::const_byte_span data) { f.write(mpt::byte_cast<const char *>(data.data()), data.size()); return !f.fail(); }
bool IsEof(std::istream & f) { return f.eof(); }
bool Flush(std::ostream & f) { f.flush(); return !f.fail(); }



} // namespace IO

} // namespace mpt



FileDataContainerSeekable::FileDataContainerSeekable(pos_type streamLength, bool buffered)
	: streamLength(streamLength)
	, cached(false)
	, m_Buffered(buffered)
	, m_Buffer(m_Buffered ? static_cast<pos_type>(BUFFER_SIZE) : 0)
{
	if(m_Buffered)
	{
		for(std::size_t chunkIndex = 0; chunkIndex < NUM_CHUNKS; ++chunkIndex)
		{
			m_ChunkIndexLRU[chunkIndex] = chunkIndex;
		}
	}
}

void FileDataContainerSeekable::CacheStream() const
{
	if(cached)
	{
		return;
	}
	if(m_Buffered)
	{
		m_Buffered = false;
		for (std::size_t chunkIndex = 0; chunkIndex < NUM_CHUNKS; ++chunkIndex)
		{
			m_ChunkInfo[chunkIndex].ChunkValid = false;
		}
		m_Buffer.resize(0);
		m_Buffer.shrink_to_fit();
	}
	cache.resize(streamLength);
	InternalRead(0, mpt::as_span(cache));
	cached = true;
}

std::size_t FileDataContainerSeekable::InternalFillPageAndReturnIndex(pos_type pos) const
{
	pos = mpt::align_down(pos, static_cast<pos_type>(CHUNK_SIZE));
	for(std::size_t chunkLRUIndex = 0; chunkLRUIndex < NUM_CHUNKS; ++chunkLRUIndex)
	{
		std::size_t chunkIndex = m_ChunkIndexLRU[chunkLRUIndex];
		if(m_ChunkInfo[chunkIndex].ChunkValid && (m_ChunkInfo[chunkIndex].ChunkOffset == pos))
		{
			std::size_t chunk = std::move(m_ChunkIndexLRU[chunkLRUIndex]);
			std::move_backward(m_ChunkIndexLRU.begin(), m_ChunkIndexLRU.begin() + chunkLRUIndex, m_ChunkIndexLRU.begin() + (chunkLRUIndex + 1));
			m_ChunkIndexLRU[0] = std::move(chunk);
			return chunkIndex;
		}
	}
	{
		std::size_t chunk = std::move(m_ChunkIndexLRU[NUM_CHUNKS - 1]);
		std::move_backward(m_ChunkIndexLRU.begin(), m_ChunkIndexLRU.begin() + (NUM_CHUNKS - 1), m_ChunkIndexLRU.begin() + NUM_CHUNKS);
		m_ChunkIndexLRU[0] = std::move(chunk);
	}
	std::size_t chunkIndex = m_ChunkIndexLRU[0];
	chunk_info& chunk = m_ChunkInfo[chunkIndex];
	chunk.ChunkOffset = pos;
	chunk.ChunkLength = InternalRead(pos, chunk_data(chunkIndex)).size();
	chunk.ChunkValid = true;
	return chunkIndex;
}

bool FileDataContainerSeekable::IsValid() const
{
	return true;
}

bool FileDataContainerSeekable::HasFastGetLength() const
{
	return true;
}

bool FileDataContainerSeekable::HasPinnedView() const
{
	return cached;
}

const std::byte *FileDataContainerSeekable::GetRawData() const
{
	CacheStream();
	return cache.data();
}

IFileDataContainer::pos_type FileDataContainerSeekable::GetLength() const
{
	return streamLength;
}

mpt::byte_span FileDataContainerSeekable::Read(IFileDataContainer::pos_type pos, mpt::byte_span dst) const
{
	if(cached)
	{
		IFileDataContainer::pos_type cache_avail = std::min(IFileDataContainer::pos_type(cache.size()) - pos, dst.size());
		std::copy(cache.begin() + pos, cache.begin() + pos + cache_avail, dst.data());
		return dst.first(cache_avail);
	} else
	{
		return InternalReadBuffered(pos, dst);
	}
}

mpt::byte_span FileDataContainerSeekable::InternalReadBuffered(pos_type pos, mpt::byte_span dst) const
{
	if(!m_Buffered)
	{
		return InternalRead(pos, dst);
	}
	pos_type totalRead = 0;
	std::byte* pdst = dst.data();
	std::size_t count = dst.size();
	while(count > 0)
	{
		std::size_t chunkIndex = InternalFillPageAndReturnIndex(pos);
		pos_type pageSkip = pos - m_ChunkInfo[chunkIndex].ChunkOffset;
		pos_type chunkWanted = std::min(static_cast<pos_type>(CHUNK_SIZE) - pageSkip, count);
		pos_type chunkGot = (m_ChunkInfo[chunkIndex].ChunkLength > pageSkip) ? (m_ChunkInfo[chunkIndex].ChunkLength - pageSkip) : 0;
		pos_type chunk = std::min(chunkWanted, chunkGot);
		std::copy(chunk_data(chunkIndex).data() + pageSkip, chunk_data(chunkIndex).data() + pageSkip + chunk, pdst);
		pos += chunk;
		pdst += chunk;
		totalRead += chunk;
		count -= chunk;
		if(chunkWanted > chunk)
		{
			return dst.first(totalRead);
		}
	}
	return dst.first(totalRead);
}



bool FileDataContainerStdStreamSeekable::IsSeekable(std::istream *stream)
{
	return mpt::IO::IsReadSeekable(*stream);
}

IFileDataContainer::pos_type FileDataContainerStdStreamSeekable::GetLength(std::istream *stream)
{
	stream->clear();
	std::streampos oldpos = stream->tellg();
	stream->seekg(0, std::ios::end);
	std::streampos length = stream->tellg();
	stream->seekg(oldpos);
	return mpt::saturate_cast<IFileDataContainer::pos_type>(static_cast<int64>(length));
}

FileDataContainerStdStreamSeekable::FileDataContainerStdStreamSeekable(std::istream *s)
	: FileDataContainerSeekable(GetLength(s), true)
	, stream(s)
{
	return;
}

mpt::byte_span FileDataContainerStdStreamSeekable::InternalRead(pos_type pos, mpt::byte_span dst) const
{
	stream->clear(); // tellg needs eof and fail bits unset
	std::streampos currentpos = stream->tellg();
	if(currentpos == std::streampos(-1) || static_cast<int64>(pos) != currentpos)
	{ // inefficient istream implementations might invalidate their buffer when seeking, even when seeking to the current position
		stream->seekg(pos);
	}
	stream->read(mpt::byte_cast<char*>(dst.data()), dst.size());
	return dst.first(static_cast<std::size_t>(stream->gcount()));
}


FileDataContainerUnseekable::FileDataContainerUnseekable()
	: cachesize(0), streamFullyCached(false)
{
	return;
}

void FileDataContainerUnseekable::EnsureCacheBuffer(std::size_t requiredbuffersize) const
{
	if(cache.size() >= cachesize + requiredbuffersize)
	{
		return;
	}
	if(cache.size() == 0)
	{
		cache.resize(mpt::align_up<std::size_t>(cachesize + requiredbuffersize, BUFFER_SIZE));
	} else if(mpt::exponential_grow(cache.size()) < cachesize + requiredbuffersize)
	{
		cache.resize(mpt::align_up<std::size_t>(cachesize + requiredbuffersize, BUFFER_SIZE));
	} else
	{
		cache.resize(mpt::exponential_grow(cache.size()));
	}
}

void FileDataContainerUnseekable::CacheStream() const
{
	if(streamFullyCached)
	{
		return;
	}
	while(!InternalEof())
	{
		EnsureCacheBuffer(BUFFER_SIZE);
		std::size_t readcount = InternalRead(mpt::span(&cache[cachesize], BUFFER_SIZE)).size();
		cachesize += readcount;
	}
	streamFullyCached = true;
}

void FileDataContainerUnseekable::CacheStreamUpTo(pos_type pos, pos_type length) const
{
	if(streamFullyCached)
	{
		return;
	}
	if(length > std::numeric_limits<pos_type>::max() - pos)
	{
		length = std::numeric_limits<pos_type>::max() - pos;
	}
	std::size_t target = mpt::saturate_cast<std::size_t>(pos + length);
	if(target <= cachesize)
	{
		return;
	}
	std::size_t alignedpos = mpt::align_up<std::size_t>(target, QUANTUM_SIZE);
	std::size_t needcount = alignedpos - cachesize;
	EnsureCacheBuffer(needcount);
	std::size_t readcount = InternalRead(mpt::span(&cache[cachesize], alignedpos - cachesize)).size();
	cachesize += readcount;
	if(!InternalEof())
	{
		// can read further
		return;
	}
	streamFullyCached = true;
}

void FileDataContainerUnseekable::ReadCached(IFileDataContainer::pos_type pos, mpt::byte_span dst) const
{
	std::copy(cache.begin() + pos, cache.begin() + pos + dst.size(), dst.data());
}

bool FileDataContainerUnseekable::IsValid() const
{
	return true;
}

bool FileDataContainerUnseekable::HasFastGetLength() const
{
	return false;
}

bool FileDataContainerUnseekable::HasPinnedView() const
{
	return true; // we have the cache which is required for seeking anyway
}

const std::byte *FileDataContainerUnseekable::GetRawData() const
{
	CacheStream();
	return cache.data();
}

IFileDataContainer::pos_type FileDataContainerUnseekable::GetLength() const
{
	CacheStream();
	return cachesize;
}

mpt::byte_span FileDataContainerUnseekable::Read(IFileDataContainer::pos_type pos, mpt::byte_span dst) const
{
	CacheStreamUpTo(pos, dst.size());
	if(pos >= IFileDataContainer::pos_type(cachesize))
	{
		return dst.first(0);
	}
	IFileDataContainer::pos_type cache_avail = std::min(IFileDataContainer::pos_type(cachesize) - pos, dst.size());
	ReadCached(pos, dst.subspan(0, cache_avail));
	return dst.subspan(0, cache_avail);
}

bool FileDataContainerUnseekable::CanRead(IFileDataContainer::pos_type pos, std::size_t length) const
{
	CacheStreamUpTo(pos, length);
	if((pos == IFileDataContainer::pos_type(cachesize)) && (length == 0))
	{
		return true;
	}
	if(pos >= IFileDataContainer::pos_type(cachesize))
	{
		return false;
	}
	return length <= IFileDataContainer::pos_type(cachesize) - pos;
}

IFileDataContainer::pos_type FileDataContainerUnseekable::GetReadableLength(IFileDataContainer::pos_type pos, std::size_t length) const
{
	CacheStreamUpTo(pos, length);
	if(pos >= cachesize)
	{
		return 0;
	}
	return std::min(static_cast<IFileDataContainer::pos_type>(cachesize) - pos, length);
}



FileDataContainerStdStream::FileDataContainerStdStream(std::istream *s)
	: stream(s)
{
	return;
}

bool FileDataContainerStdStream::InternalEof() const
{
	if(*stream)
	{
		return false;
	} else
	{
		return true;
	}
}

mpt::byte_span FileDataContainerStdStream::InternalRead(mpt::byte_span dst) const
{
	stream->read(mpt::byte_cast<char*>(dst.data()), dst.size());
	return dst.first(static_cast<std::size_t>(stream->gcount()));
}



bool FileDataContainerCallbackStreamSeekable::IsSeekable(CallbackStream stream)
{
	if(!stream.stream)
	{
		return false;
	}
	if(!stream.seek)
	{
		return false;
	}
	if(!stream.tell)
	{
		return false;
	}
	int64 oldpos = stream.tell(stream.stream);
	if(oldpos < 0)
	{
		return false;
	}
	if(stream.seek(stream.stream, 0, CallbackStream::SeekSet) < 0)
	{
		stream.seek(stream.stream, oldpos, CallbackStream::SeekSet);
		return false;
	}
	if(stream.seek(stream.stream, 0, CallbackStream::SeekEnd) < 0)
	{
		stream.seek(stream.stream, oldpos, CallbackStream::SeekSet);
		return false;
	}
	int64 length = stream.tell(stream.stream);
	if(length < 0)
	{
		stream.seek(stream.stream, oldpos, CallbackStream::SeekSet);
		return false;
	}
	stream.seek(stream.stream, oldpos, CallbackStream::SeekSet);
	return true;
}

IFileDataContainer::pos_type FileDataContainerCallbackStreamSeekable::GetLength(CallbackStream stream)
{
	if(!stream.stream)
	{
		return 0;
	}
	if(!stream.seek)
	{
		return false;
	}
	if(!stream.tell)
	{
		return false;
	}
	int64 oldpos = stream.tell(stream.stream);
	if(oldpos < 0)
	{
		return 0;
	}
	if(stream.seek(stream.stream, 0, CallbackStream::SeekSet) < 0)
	{
		stream.seek(stream.stream, oldpos, CallbackStream::SeekSet);
		return 0;
	}
	if(stream.seek(stream.stream, 0, CallbackStream::SeekEnd) < 0)
	{
		stream.seek(stream.stream, oldpos, CallbackStream::SeekSet);
		return 0;
	}
	int64 length = stream.tell(stream.stream);
	if(length < 0)
	{
		stream.seek(stream.stream, oldpos, CallbackStream::SeekSet);
		return 0;
	}
	stream.seek(stream.stream, oldpos, CallbackStream::SeekSet);
	return mpt::saturate_cast<IFileDataContainer::pos_type>(length);
}

FileDataContainerCallbackStreamSeekable::FileDataContainerCallbackStreamSeekable(CallbackStream s)
	: FileDataContainerSeekable(GetLength(s), false)
	, stream(s)
{
	return;
}

mpt::byte_span FileDataContainerCallbackStreamSeekable::InternalRead(pos_type pos, mpt::byte_span dst) const
{
	if(!stream.read)
	{
		return dst.first(0);
	}
	if(stream.seek(stream.stream, pos, CallbackStream::SeekSet) < 0)
	{
		return dst.first(0);
	}
	int64 totalread = 0;
	std::byte* pdst = dst.data();
	std::size_t count = dst.size();
	while(count > 0)
	{
		int64 readcount = stream.read(stream.stream, pdst, count);
		if(readcount <= 0)
		{
			break;
		}
		pdst += static_cast<std::size_t>(readcount);
		count -= static_cast<std::size_t>(readcount);
		totalread += readcount;
	}
	return dst.first(static_cast<std::size_t>(totalread));
}



FileDataContainerCallbackStream::FileDataContainerCallbackStream(CallbackStream s)
	: FileDataContainerUnseekable()
	, stream(s)
	, eof_reached(false)
{
	return;
}

bool FileDataContainerCallbackStream::InternalEof() const
{
	return eof_reached;
}

mpt::byte_span FileDataContainerCallbackStream::InternalRead(mpt::byte_span dst) const
{
	if(eof_reached)
	{
		return dst.first(0);
	}
	if(!stream.read)
	{
		eof_reached = true;
		return dst.first(0);
	}
	int64 totalread = 0;
	std::byte* pdst = dst.data();
	std::size_t count = dst.size();
	while(count > 0)
	{
		int64 readcount = stream.read(stream.stream, pdst, count);
		if(readcount <= 0)
		{
			eof_reached = true;
			break;
		}
		pdst += static_cast<std::size_t>(readcount);
		count -= static_cast<std::size_t>(readcount);
		totalread += readcount;
	}
	return dst.first(static_cast<std::size_t>(totalread));
}



OPENMPT_NAMESPACE_END
