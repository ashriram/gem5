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

#include "cpu/vector_engine/vpu/issue_queues/inst_queue.hh"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <queue>
#include <string>
#include <sstream>
#include <vector>

#include "debug/InstQueue.hh"
#include "debug/VectorValidBit.hh"

InstQueue::InstQueue(InstQueueParams *p) :
TickedObject(p), occupied(false),
OoO_queues(p->OoO_queues),
vector_mem_queue_size(p->vector_mem_queue_size),
vector_arith_queue_size(p->vector_arith_queue_size)
{
}

InstQueue::~InstQueue()
{
}

bool
InstQueue::isOccupied()
{
    return occupied;
}

bool
InstQueue::arith_queue_full()
{
    return (Instruction_Queue.size() >= vector_arith_queue_size);
}

bool
InstQueue::mem_queue_full()
{
    return (Memory_Queue.size() >= vector_mem_queue_size);
}

void
InstQueue::startTicking(
    VectorEngine& vector_wrapper/*,
    std::function<void()> dependencie_callback*/)
{
    this->vectorwrapper = &vector_wrapper;
    start();
}


void
InstQueue::stopTicking()
{
    DPRINTF(InstQueue,"InstQueue StopTicking \n");
    stop();
}

void
InstQueue::regStats()
{
    TickedObject::regStats();

    idle_count_by_dependency
        .name(name() + ".idle_count_by_dependency")
        .desc("Class of vector idle time because of memory")
        ;
    VectorMemQueueSlotsUsed
        .name(name() + ".VectorMemQueueSlotsUsed")
        .desc("Number of mem queue entries used during execution");
    VectorArithQueueSlotsUsed
        .name(name() + ".VectorArithQueueSlotsUsed")
        .desc("Number of arith queue entries used during execution");
}

void
InstQueue::evaluate()
{
    if ( (Instruction_Queue.size()==0) && (Memory_Queue.size()==0) )
    {
        stopTicking();
        DPRINTF(InstQueue,"Instruction Queue can not Issue more instructions"
            " because is empty \n");
        return;
    }

    if (Instruction_Queue.size()!=0 && vectorwrapper->cluster_available())
    {
        /* For statistics */
        int inst_queue_size = Instruction_Queue.size();
        if ((double)inst_queue_size > VectorArithQueueSlotsUsed.value()) {
            VectorArithQueueSlotsUsed = inst_queue_size;
        }

        uint64_t pc = 0;
        uint64_t src1=0;
        uint64_t src2=0;
        uint64_t src3=0;
        uint64_t old_dst=0;
        uint64_t mask=0;

        bool masked_op=0;
        bool vx_op=0;
        bool vf_op=0;
        bool vi_op=0;
        bool scalar_op=0;
        bool mask_ready=0;
        bool src1_ready=0;
        bool src2_ready=0;
        bool src3_ready=0;
        bool srcs_ready=0;
        QueueEntry * Instruction = Instruction_Queue.front();
        uint64_t queue_slot = 0;

        int queue_size = (OoO_queues) ? Instruction_Queue.size() : 1;
        //int queue_size = Instruction_Queue.size();

        for (int i=0 ; i< queue_size ; i++)
        {
            Instruction = Instruction_Queue[i];

            src1 = Instruction->dyn_insn->get_renamed_src1();
            src2 = Instruction->dyn_insn->get_renamed_src2();
            src3 = Instruction->dyn_insn->get_renamed_old_dst();
            old_dst = Instruction->dyn_insn->get_renamed_old_dst();
            mask = Instruction->dyn_insn->get_renamed_mask();

            masked_op = (Instruction->insn.vm()==0);

            /*
             * Instructions with Scalar operands set the src1_ready signal
             */
            vx_op = (Instruction->insn.func3()==4) || (Instruction->insn.func3()==6);
            vf_op = (Instruction->insn.func3()==5);
            vi_op = (Instruction->insn.func3()==3);
            scalar_op = vx_op || vf_op || vi_op;

            if (masked_op) {
                mask_ready = vectorwrapper->vector_reg_validbit->
                    get_preg_valid_bit(mask);
            } else {
                mask_ready = 1;
            }

            if ((Instruction->insn.arith2Srcs() ||
                Instruction->insn.arith3Srcs()) && !scalar_op) {
                src1_ready = vectorwrapper->vector_reg_validbit->
                    get_preg_valid_bit(src1);
            }
            else {
                src1_ready =1;
            }

            if (Instruction->insn.arith1Src() ||
                Instruction->insn.arith2Srcs() ||
                Instruction->insn.arith3Srcs()){
                src2_ready = vectorwrapper->vector_reg_validbit->
                    get_preg_valid_bit(src2);
            } else {
                src2_ready =1;
            }

            if (Instruction->insn.arith3Srcs()) {
                src3_ready = vectorwrapper->vector_reg_validbit->
                    get_preg_valid_bit(src3);
            } else {
                src3_ready =1;
            }

            srcs_ready = src1_ready && src2_ready && src3_ready &&
                mask_ready && !Instruction->issued;

            if (srcs_ready) {
                /* --------------------------------------------------------------------------------------------------------------
                * NEW SUPPORT
                *----------------------------------------------------------------------------------------------------------------
                * In issue stage the physical registers are asigned only to the instruction that will use it. I that way we 
                * control how the physical registers are assigned.
                *----------------------------------------------------------------------------------------------------------------
                */
                bool wb_enable = !Instruction->insn.VectorToScalar();
                uint64_t renamed_dst=1024;
                uint64_t physical_reg = 1024;
                if(vectorwrapper->vector_phy_registers->physical_frl_empty() && wb_enable) {
                DPRINTF(InstQueue,"Inst Queue can not Issue more instructions"
                    " because there are no physical registers available \n");
                return;
                }

                /* Old dst is subtracted only when the instruction is executed,
                 * because in this moment the rmt table is updated, in order to compare if the 
                 * result after subtracting is equal to zero, then push to the frl a physical reg
                 */
                if (wb_enable)
                {
                    vectorwrapper->vector_phy_registers->set_preg_comm_counter(old_dst,-1);
                    /* If the old destination count is equal to zero, then we can push the old dst to the physical FRL*/
                    if(vectorwrapper->vector_phy_registers->get_preg_comm_counter(old_dst) == 0) {
                        vectorwrapper->vector_phy_registers->set_physical_reg_frl(vectorwrapper->vector_phy_registers->get_preg_rmt(old_dst));
                    }
                }

                // CADA get_preg_rmt TAMBIEN DEBERIA TENER UN VALID, ASI POR EJEMPLO PODEMOS HACER ISSUE DE LAS QUE YA ESTAN FISICAMENTE
                // AL HACER DE TODAS, DESPUES ADELANTE SE CHECA DEPENDIENDO LA ISNTRUCCION SOO LOS NECESARIOS.

                // IMPORTANTE : PARA LIBERAR RECURSO .. PRIMERO CHECAR LSO VIEJOS PREMATUROS .... ESOS PUES SE LIBERA EL OLD DST ....
                Instruction->dyn_insn->set_physical_src1(vectorwrapper->vector_phy_registers->get_preg_rmt(src1));
                Instruction->dyn_insn->set_physical_src2(vectorwrapper->vector_phy_registers->get_preg_rmt(src2));
                Instruction->dyn_insn->set_physical_src3(vectorwrapper->vector_phy_registers->get_preg_rmt(src3));
                Instruction->dyn_insn->set_physical_old_dst(vectorwrapper->vector_phy_registers->get_preg_rmt(src3));
                Instruction->dyn_insn->set_physical_mask(vectorwrapper->vector_phy_registers->get_preg_rmt(mask));

                // Por ahora ya no se ocupa el rob para liberar ... commit counters 
                //vectorwrapper->vector_rob->set_rob_physical_old_dst(vectorwrapper->vector_phy_registers->get_preg_rmt(src3), Instruction->dyn_insn->get_rob_entry());

                //DPRINTF(InstQueue,"src1 %d ,src2 %d ,src3 %d\n",src1,src2,src3);
                //DPRINTF(InstQueue,"src1 %d ,src2 %d ,src3 %d\n",get_preg_rmt(src1),get_preg_rmt(src2),get_preg_rmt(src3));
                if (wb_enable)
                {
                /* Renamed registers are used as index to read/write the rmt memory*/
                renamed_dst = Instruction->dyn_insn->get_renamed_dst();
                physical_reg = vectorwrapper->vector_phy_registers->get_physical_reg_frl();
                vectorwrapper->vector_phy_registers->set_preg_rmt(renamed_dst , physical_reg);
                DPRINTF(InstQueue,"Arith Queue setting rmt[%d] = %d \n",renamed_dst,physical_reg);
                }

                Instruction->dyn_insn->set_physical_dst(physical_reg);
                //vectorwrapper->printArithInst(Instruction->insn,Instruction->dyn_insn,0);
                vectorwrapper->vector_phy_registers->printArithPhyInst(Instruction->insn,Instruction->dyn_insn);
                // --------------------------------------------------------------------------------------------------------------

                queue_slot = i;

                pc = Instruction->insn.getPC();
                DPRINTF(InstQueue,"Issuing arith inst %s with pc 0x%lx from queue slot %d\n",
                    Instruction->insn.getName(),*(uint64_t*)&pc,queue_slot);
                Instruction->issued = 1;
                break;
            }
        }

        if (srcs_ready)
        {
            Instruction_Queue.erase(Instruction_Queue.begin()+queue_slot);
            vectorwrapper->issue(Instruction->insn,Instruction->dyn_insn,
                Instruction->xc,Instruction->src1,Instruction->src2,
                Instruction->rename_vtype,Instruction->rename_vl,
                [Instruction,masked_op,scalar_op,src1,src2,src3,mask,this]
                (Fault f) {
                // Setting the Valid Bit
                bool wb_enable = !Instruction->insn.VectorToScalar();
                uint64_t renamed_dst = Instruction->dyn_insn->get_renamed_dst();
                if (wb_enable)
                {
                DPRINTF(VectorValidBit,"Set Valid bit to reg: %lu  with "
                    "value:%lu\n",renamed_dst,1);
                vectorwrapper->vector_reg_validbit->set_preg_valid_bit(renamed_dst,1);
                }
                //Setting the executed bit in the ROB
                uint16_t rob_entry = Instruction->dyn_insn->get_rob_entry();
                vectorwrapper->vector_rob->set_rob_entry_executed(rob_entry);

                DPRINTF(InstQueue,"Executed instruction %s\n",
                    Instruction->insn.getName());
                DPRINTF(InstQueue,"Arith Queue Size %d\n",
                    Instruction_Queue.size());
                
                /* Commit counters used to keep track the last use of the physical registers
                 * This logic will help to swap registers between memory and the physical
                 * register bank.
                 */
                if (masked_op) {
                    /* Decrease -1 mask */
                    vectorwrapper->vector_phy_registers->set_preg_comm_counter(mask,-1);
                    if(vectorwrapper->vector_phy_registers->get_preg_comm_counter(mask) == 0) {
                        vectorwrapper->vector_phy_registers->set_physical_reg_frl(Instruction->dyn_insn->get_physical_mask());
                    }
                }
                if ((Instruction->insn.arith2Srcs() ||
                    Instruction->insn.arith3Srcs()) && !scalar_op) {
                    /* Decrease -1 first source */
                    vectorwrapper->vector_phy_registers->set_preg_comm_counter(src1,-1);
                    if(vectorwrapper->vector_phy_registers->get_preg_comm_counter(src1) == 0) {
                        vectorwrapper->vector_phy_registers->set_physical_reg_frl(Instruction->dyn_insn->get_physical_src1());
                    }
                }
                if (Instruction->insn.arith1Src() ||
                    Instruction->insn.arith2Srcs() ||
                    Instruction->insn.arith3Srcs()){
                    /* Decrease -1 second source */
                    vectorwrapper->vector_phy_registers->set_preg_comm_counter(src2,-1);
                    if(vectorwrapper->vector_phy_registers->get_preg_comm_counter(src2) == 0) {
                        vectorwrapper->vector_phy_registers->set_physical_reg_frl(Instruction->dyn_insn->get_physical_src2());
                    }
                }
                if (Instruction->insn.arith3Srcs()) {
                    vectorwrapper->vector_phy_registers->set_preg_comm_counter(src3,-1);
                    if(vectorwrapper->vector_phy_registers->get_preg_comm_counter(src3) == 0) {
                        vectorwrapper->vector_phy_registers->set_physical_reg_frl(Instruction->dyn_insn->get_physical_src3());
                    }
                }
                vectorwrapper->vector_phy_registers->print_commit_counters();

                /* If the instruction writes to a scalar registers, then a callback
                 * is executed to signal that the instruction has been executed.
                 */
                if (Instruction->insn.VectorToScalar()) {
                    Instruction->dependencie_callback();
                }

                delete Instruction->dyn_insn;
                delete Instruction;
                this->occupied = false;
            });
        }
        else
        {
            idle_count_by_dependency ++;
            DPRINTF(InstQueue,"Sources not ready\n");
            uint64_t pc = Instruction->insn.getPC();
            DPRINTF(InstQueue,"Arith inst %s with pc 0x%lx \n",
            Instruction->insn.getName(),*(uint64_t*)&pc);
        }
    }
    // Stores are executed in order
    // however it is possible to advance loads OoO
    if ((Memory_Queue.size()!=0)
        && !vectorwrapper->vector_memory_unit->isOccupied()) {

        /* For statistics */
        int mem_queue_size = Memory_Queue.size();
        if ((double)mem_queue_size > VectorMemQueueSlotsUsed.value()) {
            VectorMemQueueSlotsUsed = mem_queue_size;
        }

        //occupied = true;
        uint64_t pc = 0;
        bool isStore = 0;
        bool isLoad = 0;
        uint64_t old_dst=0;
        uint64_t src3=0;
        uint64_t src2=0;
        uint8_t mop=0;
        bool indexed_op=0;
        uint64_t src_ready=0;
        bool ambiguous_dependency = 0;

        QueueEntry * Mem_Instruction = Memory_Queue.front();
        uint64_t queue_slot = 0;
        int queue_size = (OoO_queues) ? Memory_Queue.size() : 1;
        //int queue_size = Memory_Queue.size();

        //int min = std::min(queue_size ,32);
        for (int i=0 ; i< queue_size ; i++)
        {

            Mem_Instruction = Memory_Queue[i];
            isLoad = Mem_Instruction->insn.isLoad();
            isStore = Mem_Instruction->insn.isStore();
            old_dst = Mem_Instruction->dyn_insn->get_renamed_old_dst();
            src3 = Mem_Instruction->dyn_insn->get_renamed_src3();
            src2 = Mem_Instruction->dyn_insn->get_renamed_src2();
            mop = Mem_Instruction->insn.mop();
            indexed_op = (mop == 3) || (mop == 7);

            // If the instruction is indexed we stop looking for the next
            // instructions, check dependencies for indexed is too expensive
            if ( (indexed_op || isStore) && i>0){
                src_ready = 0;
                break;
            }

            /*
            // Improve it
            if ((i!=0) && isLoad)
            {
            for (int j=i ; j>0 ; j--)
                {
                QueueEntry *  Mem_Instruction_dep = Memory_Queue[j-1];
                ambiguous_dependency = ambiguous_dependency |
                    (Mem_Instruction_dep->src1 == Mem_Instruction->src1);
                delete Mem_Instruction_dep;
                }
            } */

            /* TODO : bug aqui ... debo evaluar bien los casos de indexed strided y unitstride ahora soportados*/
            src_ready = (isStore && !indexed_op) ?
                vectorwrapper->vector_reg_validbit->get_preg_valid_bit(src3) :
                (isStore && indexed_op) ?
                vectorwrapper->vector_reg_validbit->get_preg_valid_bit(src3) &&
                vectorwrapper->vector_reg_validbit->get_preg_valid_bit(src2) :
                (isLoad && !indexed_op) ?
                !Mem_Instruction->issued && !ambiguous_dependency :
                (isLoad && indexed_op) ?
                vectorwrapper->vector_reg_validbit->get_preg_valid_bit(src2):0;

            if (src_ready) {

                /* --------------------------------------------------------------------------------------------------------------
                * NEW SUPPORT
                *--------------------------------------------------------------------------------------------------------------*/
                uint64_t renamed_dst = 1024;
                uint64_t physical_reg = 1024;

                if(vectorwrapper->vector_phy_registers->physical_frl_empty() && isLoad) {
                DPRINTF(InstQueue,"Mem Queue can not Issue more instructions"
                    " because there are no physical registers available \n");
                return;
                }

                /* Old dst is subtracted only when the instruction is executed,
                 * because in this moment the rmt table is updated, in order to compare if the 
                 * result after subtracting is equal to zero, then push to the frl a physical reg
                 */
                if (isLoad) {
                    vectorwrapper->vector_phy_registers->set_preg_comm_counter(old_dst,-1);
                    /* If the old destination count is equal to zero, then we can push the old dst to the physical FRL*/
                    if(vectorwrapper->vector_phy_registers->get_preg_comm_counter(old_dst) == 0) {
                        vectorwrapper->vector_phy_registers->set_physical_reg_frl(vectorwrapper->vector_phy_registers->get_preg_rmt(old_dst));
                    }
                }

                //Mem_Instruction->dyn_insn->set_physical_src1(vectorwrapper->vector_phy_registers->get_preg_rmt(src1));
                Mem_Instruction->dyn_insn->set_physical_src2(vectorwrapper->vector_phy_registers->get_preg_rmt(src2));
                Mem_Instruction->dyn_insn->set_physical_src3(vectorwrapper->vector_phy_registers->get_preg_rmt(src3));
                Mem_Instruction->dyn_insn->set_physical_old_dst(vectorwrapper->vector_phy_registers->get_preg_rmt(old_dst));
                //Mem_Instruction->dyn_insn->set_physical_mask(vectorwrapper->vector_phy_registers->get_preg_rmt(mask));

                if (isLoad) {
                    //Por ahora ya no se ocupa el ROB para alojar el physical old, commit counters
                    //vectorwrapper->vector_rob->set_rob_physical_old_dst(vectorwrapper->vector_phy_registers->get_preg_rmt(src3), Mem_Instruction->dyn_insn->get_rob_entry());
                    /* Renamed registers are used as index to read/write the rmt memory*/
                    renamed_dst = Mem_Instruction->dyn_insn->get_renamed_dst();
                    physical_reg = vectorwrapper->vector_phy_registers->get_physical_reg_frl();
                    vectorwrapper->vector_phy_registers->set_preg_rmt(renamed_dst , physical_reg);
                    DPRINTF(InstQueue,"Mem Queue setting rmt[%d] = %d \n",renamed_dst,physical_reg);
                }

                Mem_Instruction->dyn_insn->set_physical_dst(physical_reg);
                //vectorwrapper->printMemInst(Mem_Instruction->insn,Mem_Instruction->dyn_insn);
                vectorwrapper->vector_phy_registers->printMemPhyInst(Mem_Instruction->insn,Mem_Instruction->dyn_insn);
                // --------------------------------------------------------------------------------------------------------------

                queue_slot = i;
                pc = Mem_Instruction->insn.getPC();
                DPRINTF(InstQueue,"Issuing mem inst %s with pc 0x%lx from queue slot %d\n",
                    Mem_Instruction->insn.getName(),*(uint64_t*)&pc,queue_slot);
                Mem_Instruction->issued = 1;
                break;
            }
        }

        if (src_ready)
        {
            //Memory_Queue.erase(Memory_Queue.begin()+queue_slot);
            Memory_Queue.erase(Memory_Queue.begin()+queue_slot);
            vectorwrapper->issue(Mem_Instruction->insn,
                Mem_Instruction->dyn_insn,Mem_Instruction->xc,
                Mem_Instruction->src1,Mem_Instruction->src2,
                Mem_Instruction->rename_vtype,Mem_Instruction->rename_vl,
                [Mem_Instruction,isLoad,isStore,indexed_op,src2,src3,this]
                (Fault f) {

                bool wb_enable = !Mem_Instruction->insn.isStore();
                uint64_t renamed_dst = Mem_Instruction->dyn_insn->get_renamed_dst();
                // SETTING VALID BIT
                if (wb_enable)
                {
                DPRINTF(VectorValidBit,"Set Valid bit to reg: %lu "
                    "with value:%lu\n",renamed_dst,1);
                vectorwrapper->vector_reg_validbit->set_preg_valid_bit(renamed_dst,1);
                }

                //Setting the executed bit in the ROB
                uint16_t rob_entry =
                    Mem_Instruction->dyn_insn->get_rob_entry();
                vectorwrapper->vector_rob->set_rob_entry_executed(rob_entry);

                DPRINTF(InstQueue,"Executed Mem_Instruction %s\n",
                    Mem_Instruction->insn.getName());
                DPRINTF(InstQueue,"Mem Queue Size %d\n",Memory_Queue.size());

                /* Commit counters used to keep track the last use of the physical registers
                 * This logic will help to swap registers between memory and the physical
                 * register bank.
                 */
                if (isLoad) {
                    /**Indexed operation uses the second source to hold the index values*/
                    if(indexed_op) {
                        /* Decrease -1 second source */
                        vectorwrapper->vector_phy_registers->set_preg_comm_counter(src2,-1);
                        if(vectorwrapper->vector_phy_registers->get_preg_comm_counter(src2) == 0) {
                            vectorwrapper->vector_phy_registers->set_physical_reg_frl(Mem_Instruction->dyn_insn->get_physical_src2());
                        }
                    }
                } else if (isStore) {
                    /* Decrease -1 third source */
                    vectorwrapper->vector_phy_registers->set_preg_comm_counter(src3,-1);
                    if(vectorwrapper->vector_phy_registers->get_preg_comm_counter(src3) == 0) {
                        vectorwrapper->vector_phy_registers->set_physical_reg_frl(Mem_Instruction->dyn_insn->get_physical_src3());
                    }
                    /**Indexed operation uses the second source to hold the index values*/
                    if(indexed_op) {
                        /* Decrease -1 second source */
                        vectorwrapper->vector_phy_registers->set_preg_comm_counter(src2,-1);
                        if(vectorwrapper->vector_phy_registers->get_preg_comm_counter(src2) == 0) {
                            vectorwrapper->vector_phy_registers->set_physical_reg_frl(Mem_Instruction->dyn_insn->get_physical_src2());
                        }
                    }
                }

                delete Mem_Instruction->dyn_insn;
                delete Mem_Instruction;
                this->occupied = false;
                //DPRINTF(VectorEngine,"Commit Ends\n");
                });
        }
        else
        {
            DPRINTF(InstQueue,"Sources not ready\n");
        }
    }
}

InstQueue *
InstQueueParams::create()
{
    return new InstQueue(this);
}

