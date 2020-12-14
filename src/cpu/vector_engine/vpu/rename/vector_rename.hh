/*
 * Copyright (c) 2020 Barcelona Supercomputing Center
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Cristóbal Ramírez
 */

 #ifndef __CPU_VECTOR_RENAME_H__
#define __CPU_VECTOR_RENAME_H__

#include <bitset>
#include <cstdint>
#include <deque>
#include <functional>

#include "arch/riscv/insts/vector_static_inst.hh"
#include "debug/VectorRename.hh"
#include "params/VectorRename.hh"
#include "sim/faults.hh"
#include "sim/sim_object.hh"

/**
 *  Vector Renaming
 */
class VectorRename : public SimObject
{
public:
    VectorRename(VectorRenameParams *p);
    ~VectorRename();

    const uint64_t RenamedRegs;
    const uint64_t LogicalRegs = 32;

    std::deque<uint64_t> frl_mem;
    uint64_t rat_mem[32];

    bool frl_empty()
    {
        if (frl_mem.size()==0) {
            return 1;
        }
        else{
            return 0;
        }
    }

    uint32_t frl_elements()
    {
        return frl_mem.size();
    }

    uint64_t get_frl()
    {
        if (frl_mem.size()>0) {
            uint64_t aux;
            aux = frl_mem.front();
            DPRINTF(VectorRename, "get_frl register %d\n",aux);
            frl_mem.pop_front();
            return aux;
            }
        else
        {
            DPRINTF(VectorRename, "FRL Empty\n");
            return 0;
        }
    }

    void set_frl(uint64_t reg_idx)
    {
        assert(frl_mem.size()<RenamedRegs-1);
        
        for (int i=0; i<frl_mem.size() ; i++)
        {
            assert(frl_mem[i]!= reg_idx);
        }
        DPRINTF(VectorRename, "set_frl register %d\n",reg_idx);
        frl_mem.push_back(reg_idx);

        std::stringstream texto;
        for (int i=0; i<frl_mem.size() ; i++)
        {
            texto << frl_mem[i] << " ,";
        }
        DPRINTF(VectorRename,"rename frl_mem :  %s \n",texto.str());
    }

    uint64_t get_preg_rat(uint64_t idx)
    {
        return rat_mem[idx];
    }

    void set_preg_rat(uint64_t idx , uint64_t val)
    {
        rat_mem[idx] = val;
        print_rat();
    }

    void print_rat()
    {
        std::stringstream texto;
        for (int i=0; i<32 ; i++)
        {
            texto << rat_mem[i] << "  ";
        }
        DPRINTF(VectorRename,"rename rat :  %s \n",texto.str());

    }
};



#endif // __CPU_VECTOR_RENAME_H__


