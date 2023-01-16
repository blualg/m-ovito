////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/io/SaveStream.h>
#include <ovito/core/utilities/io/LoadStream.h>

namespace Ovito {

class AnimationTime final
{
public:

    using value_type = qint64;
    Q_DECL_CONSTEXPR static value_type TicksPerFrame = 1;

    /// Default constructor.
    Q_DECL_CONSTEXPR AnimationTime() noexcept : _value(0) {}

    /// Constructs a time value from a numeric value.
    Q_DECL_CONSTEXPR explicit AnimationTime(value_type ticks) noexcept : _value(ticks) {}

    /// Returns the animation frame corresponding to this time value.   
    Q_DECL_CONSTEXPR int frame() const noexcept { return static_cast<int>(_value / TicksPerFrame); }

    /// Returns the animation time value.   
    Q_DECL_CONSTEXPR value_type ticks() const noexcept { return _value; }

    /// Equal comparison.
    Q_DECL_CONSTEXPR bool operator==(const AnimationTime& other) const noexcept { return _value == other._value; }

    /// Not-equal comparison.
    Q_DECL_CONSTEXPR bool operator!=(const AnimationTime& other) const noexcept { return _value != other._value; }

    /// Less-than comparison.
    Q_DECL_CONSTEXPR bool operator<(const AnimationTime& other) const noexcept { return _value < other._value; }

    /// Less-than-or-equal comparison.
    Q_DECL_CONSTEXPR bool operator<=(const AnimationTime& other) const noexcept { return _value <= other._value; }

    /// Greater-than comparison.
    Q_DECL_CONSTEXPR bool operator>(const AnimationTime& other) const noexcept { return _value > other._value; }

    /// Greater-than-or-equal comparison.
    Q_DECL_CONSTEXPR bool operator>=(const AnimationTime& other) const noexcept { return _value >= other._value; }

    /// Time difference.
    friend Q_DECL_CONSTEXPR value_type operator-(const AnimationTime& a, const AnimationTime& b) noexcept { return a._value - b._value; }

    /// Time addition.
    friend Q_DECL_CONSTEXPR AnimationTime operator+(const AnimationTime& a, value_type delta) noexcept { return AnimationTime(a._value + delta); }

    /// Time subtraction.
    friend Q_DECL_CONSTEXPR AnimationTime operator-(const AnimationTime& a, value_type delta) noexcept { return AnimationTime(a._value - delta); }

    /// Returns the smallest time value (negative infinity).
    static Q_DECL_CONSTEXPR AnimationTime negativeInfinity() noexcept { return AnimationTime(std::numeric_limits<value_type>::lowest()); }

    /// Returns the largest time value (positive infinity).
    static Q_DECL_CONSTEXPR AnimationTime positiveInfinity() noexcept { return AnimationTime(std::numeric_limits<value_type>::max()); }

    /// Constructs a time value corresponding to the given frame.   
    static Q_DECL_CONSTEXPR AnimationTime fromFrame(int frame) noexcept { return AnimationTime(TicksPerFrame * static_cast<value_type>(frame)); }

    /// \brief Writes an animation time to a binary output stream.
    /// \param stream The output stream.
    /// \param time The time to write to the output stream \a stream.
    /// \return The output stream \a stream.
    friend SaveStream& operator<<(SaveStream& stream, const AnimationTime& time) {
        return stream << time.ticks();
    }

    /// \brief Reads an animation time value from a binary input stream.
    /// \param stream The input stream.
    /// \param time Reference to a variable where the parsed data will be stored.
    /// \return The input stream \a stream.
    friend LoadStream& operator>>(LoadStream& stream, AnimationTime& time) {
        if(stream.formatVersion() >= 30009) {
            stream >> time._value;
        }
        else {
            // Backward compatibility with OVITO 3.7: Old 'TimePoint' data type was a simple int, and time
            // was measured in ticks (4800 ticks per second).
            int timePoint;
            stream >> timePoint;
            time._value = timePoint;
        }
        return stream;
    }

    /// \brief Writes a time value to the debug stream.
    friend QDebug operator<<(QDebug stream, const AnimationTime& time) {
        stream.nospace() << time.ticks() << " (frame " << time.frame() << ")";
        return stream.space();
    }

private:

    value_type _value;
};

/**
 * \brief An interval on the animation time line, which is defined by a start and an end time.
 */
class TimeInterval
{
public:

    /// \brief Creates an empty time interval.
    ///
    /// Both start time and end time are initialized to negative infinity.
    Q_DECL_CONSTEXPR TimeInterval() noexcept : _start{AnimationTime::negativeInfinity()}, _end{AnimationTime::negativeInfinity()} {}

    /// \brief Initializes the interval with start and end values.
    /// \param start The start time of the time interval.
    /// \param end The end time (including) of the time interval.
    Q_DECL_CONSTEXPR TimeInterval(AnimationTime start, AnimationTime end) noexcept : _start(start), _end(end) {}

    /// \brief Initializes the interval to an instant time.
    /// \param time The time where the interval starts and ends.
    Q_DECL_CONSTEXPR TimeInterval(AnimationTime time) noexcept : _start(time), _end(time) {}

    /// \brief Returns the start time of the interval.
    /// \return The beginning of the time interval.
    Q_DECL_CONSTEXPR AnimationTime start() const noexcept { return _start; }

    /// \brief Returns the end time of the interval.
    /// \return The time at which the interval end.
    Q_DECL_CONSTEXPR AnimationTime end() const noexcept { return _end; }

    /// \brief Sets the start time of the interval.
    /// \param start The new start time.
    void setStart(AnimationTime start) noexcept { _start = start; }

    /// \brief Sets the end time of the interval.
    /// \param end The new end time.
    void setEnd(AnimationTime end) noexcept { _end = end; }

    /// \brief Checks if this is an empty time interval.
    /// \return \c true if the start time of the interval is behind the end time or if the
    ///         end time is negative infinity;
    ///         \c false otherwise.
    /// \sa setEmpty()
    Q_DECL_CONSTEXPR bool isEmpty() const noexcept { return (end() == AnimationTime::negativeInfinity() || start() > end()); }

    /// \brief Returns whether this is the infinite time interval.
    /// \return \c true if the start time is negative infinity and the end time of the interval is positive infinity.
    /// \sa setInfinite()
    Q_DECL_CONSTEXPR bool isInfinite() const noexcept { return (end() == AnimationTime::positiveInfinity() && start() == AnimationTime::negativeInfinity()); }

    /// \brief Returns the duration of the time interval.
    /// \return The difference between the end and the start time.
    /// \sa setDuration()
    Q_DECL_CONSTEXPR AnimationTime::value_type duration() const noexcept { return end() - start(); }

    /// \brief Sets the duration of the time interval.
    /// \param duration The new duration of the interval.
    ///
    /// This method changes the end time of the interval to be
    /// start() + duration().
    ///
    /// \sa duration()
    void setDuration(AnimationTime::value_type duration) noexcept { setEnd(start() + duration); }

    /// \brief Sets this interval's start time to negative infinity and it's end time to positive infinity.
    /// \sa isInfinite()
    void setInfinite() noexcept {
        setStart(AnimationTime::negativeInfinity());
        setEnd(AnimationTime::positiveInfinity());
    }

    /// \brief Sets this interval's start and end time to negative infinity.
    /// \sa isEmpty()
    void setEmpty() noexcept {
        setStart(AnimationTime::negativeInfinity());
        setEnd(AnimationTime::negativeInfinity());
    }

    /// \brief Sets this interval's start and end time to the instant time given.
    /// \param time This value is assigned to both, the start and the end time of the interval.
    void setInstant(AnimationTime time) noexcept {
        setStart(time);
        setEnd(time);
    }

    /// \brief Compares two intervals for equality.
    /// \param other The interval to compare with.
    /// \return \c true if start and end time of both intervals are equal.
    bool operator==(const TimeInterval& other) const noexcept { return (other.start() == start() && other.end() == end()); }

    /// \brief Compares two intervals for inequality.
    /// \param other The interval to compare with.
    /// \return \c true if start or end time of both intervals are not equal.
    bool operator!=(const TimeInterval& other) const noexcept { return (other.start() != start() || other.end() != end()); }

    /// \brief Assignment operator.
    /// \param other The interval to copy.
    /// \return This interval instance.
    TimeInterval& operator=(const TimeInterval& other) noexcept {
        setStart(other.start());
        setEnd(other.end());
        return *this;
    }

    /// \brief Returns whether a time lies between start and end time of this interval.
    /// \param time The time to check.
    /// \return \c true if \a time is equal or larger than start() and smaller or equal than end().
    Q_DECL_CONSTEXPR bool contains(AnimationTime time) const noexcept {
        return (start() <= time && time <= end());
    }

    /// \brief Intersects this interval with the another one.
    /// \param other Another time interval.
    ///
    /// Start and end time of this interval are such that they include the interval \a other as well as \c this interval.
    void intersect(const TimeInterval& other) noexcept {
        if(end() < other.start()
            || start() > other.end()
            || other.isEmpty()) {
            setEmpty();
        }
        else if(!other.isInfinite()) {
            setStart(std::max(start(), other.start()));
            setEnd(std::min(end(), other.end()));
            OVITO_ASSERT(start() <= end());
        }
    }

    /// Tests if two time interval overlap (either full or partially).
    Q_DECL_CONSTEXPR bool overlap(const TimeInterval& iv) const noexcept {
        if(isEmpty() || iv.isEmpty()) return false;
        if(start() >= iv.start() && start() <= iv.end()) return true;
        if(end() >= iv.start() && end() <= iv.end()) return true;
        return (iv.start() >= start() && iv.start() <= end());
    }

    /// Return the infinite time interval that contains all time values.
    static Q_DECL_CONSTEXPR TimeInterval infinite() noexcept { return TimeInterval(AnimationTime::negativeInfinity(), AnimationTime::positiveInfinity()); }

    /// Return the empty time interval that contains no time values.
    static Q_DECL_CONSTEXPR TimeInterval empty() noexcept { return TimeInterval(); }

    /// \brief Writes a time interval to a binary output stream.
    /// \param stream The output stream.
    /// \param iv The time interval to write to the output stream \a stream.
    /// \return The output stream \a stream.
    /// \relates TimeInterval
    friend inline SaveStream& operator<<(SaveStream& stream, const TimeInterval& iv) {
        return stream << iv.start() << iv.end();
    }

    /// \brief Reads a time interval from a binary input stream.
    /// \param stream The input stream.
    /// \param iv Reference to a variable where the parsed data will be stored.
    /// \return The input stream \a stream.
    /// \relates TimeInterval
    friend inline LoadStream& operator>>(LoadStream& stream, TimeInterval& iv) {
        stream >> iv._start >> iv._end;
        return stream;
    }

    /// \brief Writes a time interval to the debug stream.
    /// \relates TimeInterval
    friend inline QDebug operator<<(QDebug stream, const TimeInterval& iv) {
        stream.nospace() << "[" << iv.start() << ", " << iv.end() << "]";
        return stream.space();
    }

private:

    AnimationTime _start, _end;
};

/**
 * This data structure manages the union of multiple, non-overlapping animation time intervals.
 */
class TimeIntervalUnion : private QVarLengthArray<TimeInterval, 2>
{
private:

    using base_class = QVarLengthArray<TimeInterval, 2>;

public:

    using base_class::const_iterator;
    using base_class::reverse_iterator;
    using base_class::iterator;
    using base_class::reference;
    using base_class::size_type;
    using base_class::value_type;

    /// Constructs an empty union of intervals.
    TimeIntervalUnion() = default;

    /// Constructs a union that includes only the given animation time instant.
    explicit TimeIntervalUnion(AnimationTime time) : base_class{{ TimeInterval(time) }} {}

    /// Add a time interval to the union.
    void add(TimeInterval iv) {
        if(iv.isEmpty()) return;

        // Subtract existing intervals from interval to be added.
        for(iterator iter = begin(); iter != end(); ) {
            // Erase existing intervals that are completely contained in the interval to be added.
            if(iv.start() <= iter->start() && iv.end() >= iter->end()) {
                iter = erase(iter);
            }
            else {
                if(iv.start() >= iter->start() && iv.start() <= iter->end())
                    iv.setStart(iter->end() + 1);
                if(iv.end() >= iter->start() && iv.end() <= iter->end())
                    iv.setEnd(iter->start() - 1);
                if(iv.isEmpty())
                    return;
                ++iter;
            }
        }
        push_back(iv);

        // TODO: Merge adjacent time intervals.
    }

    // Inherited const methods from QVarLengthArray.
    using base_class::begin;
    using base_class::end;
    using base_class::cbegin;
    using base_class::cend;
    using base_class::rbegin;
    using base_class::rend;
    using base_class::crbegin;
    using base_class::crend;
    using base_class::clear;
    using base_class::size;
    using base_class::empty;
    using base_class::front;
    using base_class::back;
};

/// \brief Writes a union of time intervals to the debug stream.
/// \relates TimeIntervalUnion
inline QDebug operator<<(QDebug stream, const TimeIntervalUnion& ivu)
{
    QDebug dbg = stream.nospace();
    dbg << "{";
    for(const TimeInterval& iv : ivu)
        dbg << "[" << iv.start() << "-" << iv.end() << "]";
    dbg << "}";
    return stream.space();
}

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::AnimationTime);
Q_DECLARE_TYPEINFO(Ovito::AnimationTime, Q_PRIMITIVE_TYPE);

Q_DECLARE_METATYPE(Ovito::TimeInterval);
Q_DECLARE_TYPEINFO(Ovito::TimeInterval, Q_RELOCATABLE_TYPE);
