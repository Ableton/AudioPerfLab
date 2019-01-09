// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <algorithm>
#include <atomic>
#include <memory>

/*! A fixed-size single-producer single-consumer queue.
 *
 * This is a classic ringbuffer (circular array), with support for move-only types,
 * in-place construction and access, and allocators.
 */
template <class T, class Allocator = std::allocator<T>>
class FixedSPSCQueue
{
public:
  using value_type = T;
  using size_type = uint32_t;

  /*! Construct a queue.
   *
   * @param bufferSize The size of the internal array, as an element count.  This is
   * rounded up to the next power of two if necessary, and the queue can store one less
   * than this number of elements.  That is, an array of nextPowerOfTwo(bufferSize)
   * elements will be allocated, and the queue can only contain nextPowerOfTwo(bufferSize)
   * - 1 elements at a time.
   *
   * @param alloc Allocator used for allocating and deallocating the array on construction
   * and destruction (not used for elements).
   */
  explicit FixedSPSCQueue(uint32_t bufferSize, const Allocator& alloc = Allocator())
    : mAlloc(alloc)
    , mSize(std::max(2U, nextPowerOfTwo(bufferSize)))
    , mSizeMask(mSize - 1)
    , mpArray(mAlloc.allocate(mSize))
    , mReadIndex(0)
    , mWriteIndex(0)
  {
  }

  ~FixedSPSCQueue()
  {
    if (!std::is_trivially_destructible<T>::value)
    {
      const uint32_t end = mWriteIndex;
      for (uint32_t r = mReadIndex; r != end; r = nextIndex(r))
      {
        mpArray[r].~T();
      }
    }

    mAlloc.deallocate(mpArray, mSize);
  }

  FixedSPSCQueue(const FixedSPSCQueue&) = delete;
  FixedSPSCQueue& operator=(const FixedSPSCQueue&) = delete;

  /*! Try to push a new element to the back of the queue.
   *
   * Wait-free, one acquire barrier, one release barrier, but may fail.
   *
   * Note that the empty() method should not be used by the writer, instead, attempt to
   * push and check the return value for success.
   *
   * @param args Arguments forwarded to the element constructor, like T(args).
   *
   * @return True on success, false if the queue is full.
   */
  template <class... Args>
  bool tryPushBack(Args&&... args)
  {
    const uint32_t thisWrite = mWriteIndex.load(std::memory_order_relaxed);
    const uint32_t nextWrite = nextIndex(thisWrite);
    if (nextWrite == mReadIndex.load(std::memory_order_acquire))
    {
      return false; // full
    }

    new (&mpArray[thisWrite]) T(std::forward<Args>(args)...);
    mWriteIndex.store(nextWrite, std::memory_order_release);
    return true;
  }

  /*! Pop the element off the front of the queue.
   *
   * Wait-free, one acquire barrier, one release barrier.
   *
   * @return True on success, false if the queue is empty.
   */
  bool popFront()
  {
    const uint32_t thisRead = mReadIndex.load(std::memory_order_relaxed);
    if (thisRead == mWriteIndex.load(std::memory_order_acquire))
    {
      return false; // empty
    }

    const uint32_t nextRead = nextIndex(thisRead);
    mpArray[thisRead].~T();
    mReadIndex.store(nextRead, std::memory_order_release);
    return true;
  }

  /*! Return a pointer to the element at the front of the queue.
   *
   * Wait-free, one acquire barrier.
   *
   * @return A pointer to the front element, or null if the queue is empty.
   */
  T* front() const
  {
    const uint32_t thisRead = mReadIndex.load(std::memory_order_relaxed);
    if (thisRead == mWriteIndex.load(std::memory_order_acquire))
    {
      return nullptr; // empty
    }

    return &mpArray[thisRead];
  }

  /*! Return the number of elements that can be enqueued at once. */
  uint32_t capacity() const { return mSizeMask; }

  /*! Return true iff the queue is empty.
   *
   * Note that this method should not normally be used, the consumer should check the
   * front() immediately rather than check for emptiness first.
   */
  bool empty() const { return mReadIndex.load() == mWriteIndex.load(); }

private:
  //! Fast circular increment that exploits 2^k size to avoid branching or divison
  inline uint32_t nextIndex(uint32_t index) { return (index + 1) & mSizeMask; }

  ///! http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
  static inline uint32_t nextPowerOfTwo(uint32_t size)
  {
    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size++;
    return size;
  }

  Allocator mAlloc;         //!< Allocator (used only during construction/destruction)
  const uint32_t mSize;     //!< One more than the possible number of elements
  const uint32_t mSizeMask; //!< Mask for fast modulo
  T* const mpArray;         //!< Array of queue elements

  std::atomic<uint32_t> mReadIndex; //!< Read index, modified by reader only

  //! Padding to push mWriteIndex to the next cache line
  const char mPad1[kCacheLineSize - sizeof(std::atomic<uint32_t>)] = {};

  std::atomic<uint32_t> mWriteIndex; //!< Write index, modified by writer only

  //! Padding to prevent destructive interference with the next object
  const char mPad2[kCacheLineSize - sizeof(std::atomic<uint32_t>)] = {};
};
