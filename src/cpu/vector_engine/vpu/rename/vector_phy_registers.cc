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

#include "cpu/vector_engine/vpu/rename/vector_phy_registers.hh"

#include <bitset>
#include <cstdint>
#include <deque>
#include <functional>

#include "debug/VectorPhyRegisters.hh"

/**
 *  Vector Renaming
 */
VectorPhyRegisters::VectorPhyRegisters(VectorPhyRegistersParams *p) :
SimObject(p), RenamedRegs(p->RenamedRegs), PhysicalRegs(p->PhysicalRegs)
{
    // 1024 means that the register is not used ... for memory location we should define other number ???
    for (uint64_t i=0; i<RenamedRegs; i++) {
            physical_rmt_mem.push_back(1024);
        }
    //PhysicalRegs refers to the number of real physical registers (not the renamed ones)
    for (uint64_t i=0; i<PhysicalRegs; i++) {
            physical_frl_mem.push_back(i);
        }
    //Commit counters
    for (uint64_t i=0; i<32; i++)
        {
            commit_counters.push_back(1);
        }
    for (uint64_t i=32; i<RenamedRegs; i++)
        {
            commit_counters.push_back(0);
        }
}

VectorPhyRegisters::~VectorPhyRegisters()
{
}


uint64_t
VectorPhyRegisters::get_preg_rmt(uint64_t idx)
{
    assert((idx < RenamedRegs) || (idx==1024));

    if (idx==1024) {
        return 1024;
    } else {
        return physical_rmt_mem[idx];
    }
}

void
VectorPhyRegisters::set_preg_rmt(uint64_t idx , uint64_t val)
{
    assert(val<PhysicalRegs);
    assert( (idx < RenamedRegs) || (idx==1024));

    physical_rmt_mem[idx] = val;
    DPRINTF(VectorPhyRegisters,"physical_rmt_mem [%d] = %d\n",idx,val);

    std::stringstream texto;
    for (int i=0; i<physical_rmt_mem.size() ; i++)
    {
        texto << physical_rmt_mem[i] << " ,";
    }
    DPRINTF(VectorPhyRegisters,"physical rmt_mem %s \n",texto.str());
}

bool 
VectorPhyRegisters::physical_frl_empty()
{
    return (physical_frl_mem.size()==0);
}

uint64_t 
VectorPhyRegisters::get_physical_reg_frl()
{
    if (physical_frl_mem.size()>0) {
        uint64_t aux;
        aux = physical_frl_mem.front();
        physical_frl_mem.pop_front();
        DPRINTF(VectorPhyRegisters,"get_physical_reg_frl %d \n",aux);
        return aux;
        }
    else
    {
        DPRINTF(VectorPhyRegisters, "FRL Empty\n");
        return 0;
    }
}

void 
VectorPhyRegisters::set_physical_reg_frl(uint64_t reg_idx)
{
    if (reg_idx == 1024) { return;}
    assert(physical_frl_mem.size()<PhysicalRegs-1);
    // Repeated free register is not allowed
    for (int i=0; i<physical_frl_mem.size() ; i++)
    {
        assert(physical_frl_mem[i]!= reg_idx);
    }

    physical_frl_mem.push_back(reg_idx);
    DPRINTF(VectorPhyRegisters,"Pushing physical reg %d to the frl\n",reg_idx);

    std::stringstream texto;
    for (int i=0; i<physical_frl_mem.size() ; i++)
    {
        texto << physical_frl_mem[i] << " ,";
    }
    DPRINTF(VectorPhyRegisters,"physical frl_mem %s \n",texto.str());

}



void
VectorPhyRegisters::printMemPhyInst(RiscvISA::VectorStaticInst& insn,VectorDynInst *vector_dyn_insn)
{
    //uint64_t pc = insn.getPC();
    bool indexed = (insn.mop() ==3);

    uint32_t phy_dst = vector_dyn_insn->get_physical_dst();
    uint32_t phy_old_dst = vector_dyn_insn->get_physical_old_dst();
    uint32_t phy_vs2 = vector_dyn_insn->get_physical_src2();
    uint32_t phy_vs3 = vector_dyn_insn->get_physical_src3();
    uint32_t phy_mask = vector_dyn_insn->get_physical_mask();

    bool masked_op = (insn.vm()==0);

    std::stringstream mask_phy;
    if (masked_op) {
        mask_phy << "v" << phy_mask << ".m";
    } else {
        mask_phy << "";
    }
    if (insn.isLoad())
    {
        if (indexed){
            DPRINTF(VectorPhyRegisters,"physical inst: %s v%d v%d %s  old_dst v%d\n",insn.getName(),phy_dst,phy_vs2,mask_phy.str(),phy_old_dst);
        } else {
            DPRINTF(VectorPhyRegisters,"physical inst: %s v%d %s  old_dst v%d\n",insn.getName(),phy_dst,mask_phy.str(),phy_old_dst);
        }
    }
    else if (insn.isStore())
    {
         if (indexed){
            DPRINTF(VectorPhyRegisters,"physical inst: %s v%d v%d %s\n",insn.getName(),phy_vs3,phy_vs2,mask_phy.str());
        } else {
            DPRINTF(VectorPhyRegisters,"physical inst: %s v%d %s\n",insn.getName(),phy_vs3,mask_phy.str());
        }
        
    } else {
        panic("Invalid Vector Instruction insn=%#h\n", insn.machInst);
    }
}

void
VectorPhyRegisters::printArithPhyInst(RiscvISA::VectorStaticInst& insn,VectorDynInst *vector_dyn_insn)
{
    //uint64_t pc = insn.getPC();

    bool vx_op = (insn.func3()==4) || (insn.func3()==6);
    bool vf_op = (insn.func3()==5);
    bool vi_op = (insn.func3()==3);
    bool masked_op = (insn.vm()==0);

    std::string reg_type;
    if(insn.VectorToScalar()==1) {reg_type = "x";} else {reg_type = "v";}

    std::string scr1_type;
    scr1_type = (vx_op) ? "x" :
                (vf_op) ? "f" :
                (vi_op) ? " " : "v";

    uint32_t phy_dst = (insn.VectorToScalar()==1) ? insn.vd() : vector_dyn_insn->get_physical_dst();
    uint32_t phy_old_dst = vector_dyn_insn->get_physical_old_dst();
    uint32_t phy_vs1 = (vx_op || vf_op || vi_op) ? insn.vs1() : vector_dyn_insn->get_physical_src1();
    uint32_t phy_vs2 = vector_dyn_insn->get_physical_src2();
    uint32_t phy_mask = vector_dyn_insn->get_physical_mask();

    std::stringstream mask_phy;
    if (masked_op) {
        mask_phy << "v" << phy_mask << ".m";
    } else {
        mask_phy << "";
    }

    if (insn.arith1Src()) {
        DPRINTF(VectorPhyRegisters,"physical inst: %s %s%d v%d %s  old_dst v%d\n",insn.getName(),reg_type,phy_dst,phy_vs2,mask_phy.str(),phy_old_dst);
    }
    else if (insn.arith2Srcs()) {
        DPRINTF(VectorPhyRegisters,"physical inst: %s %s%d v%d %s%d %s  old_dst v%d\n",insn.getName(),reg_type,phy_dst,phy_vs2,scr1_type,phy_vs1,mask_phy.str(),phy_old_dst);
    }
    else if (insn.arith3Srcs()) {
        DPRINTF(VectorPhyRegisters,"physical inst: %s %s%d v%d %s%d v%d %s old_dst v%d\n",insn.getName(),reg_type,phy_dst,phy_vs2,scr1_type,phy_vs1,phy_old_dst,mask_phy.str(),phy_old_dst);
    } else {
        panic("Invalid Vector Instruction insn=%#h\n", insn.machInst);
    }
}

void 
VectorPhyRegisters::set_preg_comm_counter(int idx , int val) 
{ 
    if(idx==1024) {return;}
    DPRINTF(VectorPhyRegisters,"Setting the commit_counter %d with %d \n",idx,val);
    assert(idx <= RenamedRegs);
    commit_counters[idx] = commit_counters[idx] + val;
}

int 
VectorPhyRegisters::get_preg_comm_counter(int idx)
{
    //if(idx==1024) {return 1;}
    assert((idx <= RenamedRegs));
    return commit_counters[idx]; 
}

void 
VectorPhyRegisters::print_commit_counters() { 
    std::stringstream counters;
    for(int i=0;i<RenamedRegs;i++) {
        counters << "slot "<< i << ": " << commit_counters[i] << "\n";
    }
    DPRINTF(VectorPhyRegisters,"COMMIT COUNTERS VECTOR\n %s",counters.str());
}

VectorPhyRegisters *
VectorPhyRegistersParams::create()
{
    return new VectorPhyRegisters(this);
}
