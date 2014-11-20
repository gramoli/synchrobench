//------------------------------------------------------------------------------
// File    : SnapshotCounter.java
// Author  : Ms.Moran Tzafrir
// Written : 13 April 2009
//
// Multi-Platform C++ framework
//
// Copyright (C) 2009 Moran Tzafrir, Nir Shavit.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//------------------------------------------------------------------------------
// Fixed:
//------------------------------------------------------------------------------
// TODO:
//------------------------------------------------------------------------------

package linkedlists.lockbased.lazyutils;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

public final class SnapshotCounter {

    //constants -----------------------------------
    private final int   _NUM_PROCESS;

    //helper functions ----------------------------
    private static final int get_size(final long counterAndValue) {
        return (int)(counterAndValue & 0xFFFFFFFFL);
    }
    private static final int get_seq(final long counterAndValue) {
        return (int)(counterAndValue >>> 32);
    }
    private static final long build_seq_value(final int seq, final int value) {
        return (((long)seq << 32) | (long)value);
    }

    //inner classes -------------------------------
    private static final class process_data {
        private long _recent_seq_value;
        private long _prev_seq_value;

        process_data() {
            _recent_seq_value = 0;
            _prev_seq_value = 0;
        }

        int recent_seq() {
            return get_seq(_recent_seq_value);
        }
        int recent_seq_int() {
            return get_seq(_recent_seq_value);
        }
        int prev_seq() {
            return get_seq(_prev_seq_value);
        }
        int recent_value() {
            return get_size(_recent_seq_value);
        }
        int prev_value() {
            return get_size(_prev_seq_value);
        }
        void update(final int counter, final int value) {
            _recent_seq_value = build_seq_value(counter, value);
        }
        void update_prev(final int counter, final int value) {
            _prev_seq_value = build_seq_value(counter, value);
        }
        void set_prev_to_curr() {
            _prev_seq_value = _recent_seq_value;
        }
    }

    //fields --------------------------------------
    private process_data    gMem[];
    private AtomicInteger   gSeq;
    private AtomicLong      gView;

    //public operations ---------------------------
    public SnapshotCounter(final int num_process) {
        _NUM_PROCESS = num_process;
        gSeq = new AtomicInteger(1);
        gView = new AtomicLong(0);

        gMem = new process_data[_NUM_PROCESS];
        for(int iProcess=0; iProcess < _NUM_PROCESS; ++iProcess) {
            gMem[iProcess] = new process_data();
        }
    }
    public void update(final int iProcess, final int new_value) {
        final process_data pdata = gMem[iProcess];
        final int update_seq = gSeq.get();
        if (update_seq != pdata.recent_seq())
            pdata.set_prev_to_curr();
        pdata.update(update_seq, new_value);
    }
    public void inc(final int iProcess) {
        final process_data pdata = gMem[iProcess];
        final int update_seq = gSeq.get();
        if (update_seq != pdata.recent_seq())
            pdata.set_prev_to_curr();
        pdata.update(update_seq, pdata.recent_value()+1);
    }
    public void dec(final int iProcess) {
        final process_data pdata = gMem[iProcess];
        final int update_seq = gSeq.get();
        if (update_seq != pdata.recent_seq())
            pdata.set_prev_to_curr();
        pdata.update(update_seq, pdata.recent_value()-1);
    }
    public void add(final int iProcess, final int x) {
        final process_data pdata = gMem[iProcess];
        final int update_seq = gSeq.get();
        if (update_seq != pdata.recent_seq())
            pdata.set_prev_to_curr();
        pdata.update(update_seq, pdata.recent_value()+x);
    }
    public int valueRequest(final int iProcess) {
        return (int)(gMem[iProcess].recent_value());
    }

    @SuppressWarnings("empty-statement")
    public int scan_sum() {
        final int init_seq = gSeq.get();
        if(init_seq > 2) {
            int iTry=0;
            final int max_wait = (_NUM_PROCESS*2000);
            while((init_seq > (get_seq(gView.get()))) && iTry<max_wait)  {++iTry;--iTry; ++iTry;--iTry;++iTry;}
            if(init_seq < get_seq(gView.get()))
                return get_size(gView.get());
        }

        long start_view = gView.get();
        int scan_seq = gSeq.incrementAndGet();
        final int first_seq = scan_seq;
        do {
            int size=0;
            boolean scan_ok=true;
            for(int iProcess=0; iProcess < _NUM_PROCESS; ++iProcess) {
                final process_data pdata = gMem[iProcess];
                final long prev = pdata._prev_seq_value;
                final long recent = pdata._recent_seq_value;

                if (get_seq(recent) < scan_seq)
                    size += pdata.recent_value();
                else if (get_seq(recent) == scan_seq)
                    size += get_size(prev);
                else {
                    scan_ok=false;
                    start_view = gView.get();
                    scan_seq = pdata.recent_seq();
                    break;
                }
            }

            if(first_seq <= get_seq(gView.get()))
                return get_size(gView.get());
            if(scan_ok) {
                if(gView.compareAndSet(start_view, build_seq_value(scan_seq, size)))
                    return size;
                start_view = gView.get();
            }
        } while (true);
    }
}
