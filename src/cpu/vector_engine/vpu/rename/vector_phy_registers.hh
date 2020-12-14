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

 #ifndef __CPU_VECTOR_PHY_REGS_H__
#define __CPU_VECTOR_PHY_REGS_H__

#include <bitset>
#include <cstdint>
#include <deque>
#include <functional>

#include "arch/riscv/insts/vector_static_inst.hh"
#include "cpu/vector_engine/vector_dyn_inst.hh"
#include "debug/VectorPhyRegisters.hh"
#include "params/VectorPhyRegisters.hh"
#include "sim/faults.hh"
#include "sim/sim_object.hh"

/**
 *  Vector Physical Registers
 */
class VectorPhyRegisters : public SimObject
{
public:
    VectorPhyRegisters(VectorPhyRegistersParams *p);
    ~VectorPhyRegisters();

    const uint64_t RenamedRegs;
    const uint64_t PhysicalRegs;

    int get_preg_comm_counter(int idx);
    void set_preg_comm_counter(int idx , int val);
    void print_commit_counters();

    /* Functions for the new register allocation */
    uint64_t get_preg_rmt(uint64_t idx);
    void set_preg_rmt(uint64_t idx , uint64_t val);
    bool physical_frl_empty();
    uint64_t get_physical_reg_frl();
    void set_physical_reg_frl(uint64_t reg_idx);

    /* Print functions */
    void printMemPhyInst(RiscvISA::VectorStaticInst& insn,VectorDynInst *vector_dyn_insn);
    void printArithPhyInst(RiscvISA::VectorStaticInst& insn,VectorDynInst *vector_dyn_insn);

private:
    /* Commit counters*/
    std::vector<int> commit_counters;

    std::deque<uint64_t> physical_rmt_mem;
    std::deque<uint64_t> physical_frl_mem;

};



#endif // __CPU_VECTOR_PHY_REGS_H__


