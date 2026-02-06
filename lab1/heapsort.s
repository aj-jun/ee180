#==============================================================================
# File:         heapsort.s (PA 1)
#
# Description:  Skeleton for assembly heapsort routine.
#
#       To complete this assignment, add the following functionality:
#
#       1. Call heapsort from main. (See heapsort.c)
#          Pass 2 arguments:
#
#          ARG 1: Pointer to the first element of the array
#          (referred to as "array" in the C code)
#
#          ARG 2: Number of elements in the array
#
#          Remember to use the correct CALLING CONVENTIONS !!!
#          Pass all arguments in the conventional way!
#
#       2. Implement heapsort
#          heapsort should call build_max_heap, then repeatedly swap the root
#          with the last element and call heapify on the shrinked heap.
#
#       3. Implement helper functions:
#          - build_max_heap
#          - heapify   (RECURSIVE: heapify MUST call itself)
#          - swap
#
#       Notes:
#         - We use a 0-indexed max-heap.
#         - For index i:
#             left  = 2*i + 1
#             right = 2*i + 2
#             parent= (i-1)/2
#         - You do not need to use $fp for this assignment.
#
#==============================================================================

.data
HOW_MANY:   .asciiz "How many elements to be sorted? "
ENTER_ELEM: .asciiz "Enter next element: "
ANS:        .asciiz "The sorted list is:\n"
SPACE:      .asciiz " "
EOL:        .asciiz "\n"

.text
.globl main

#==========================================================================
main:
#==========================================================================

    addiu   $sp, $sp, -32
    sw      $ra, 28($sp)

    li      $v0, 4
    la      $a0, HOW_MANY
    syscall
    li      $v0, 5
    syscall
    move    $s1, $v0

    li      $v0, 9
    sll     $s2, $s1, 2
    move    $a0, $s2
    syscall
    move    $s0, $v0

    addu    $t1, $s0, $s2
    move    $t0, $s0
    j       read_loop_cond

read_loop:
    li      $v0, 4
    la      $a0, ENTER_ELEM
    syscall
    li      $v0, 5
    syscall
    sw      $v0, 0($t0)
    addiu   $t0, $t0, 4

read_loop_cond:
    bne     $t0, $t1, read_loop

    move    $a0, $s0
    move    $a1, $s1
    blez    $s1, return
    jal     heapsort

return:
    li      $v0, 4
    la      $a0, ANS
    syscall

    move    $t0, $s0
    addu    $t1, $s0, $s2
    j       print_loop_cond

print_loop:
    li      $v0, 1
    lw      $a0, 0($t0)
    syscall
    li      $v0, 4
    la      $a0, SPACE
    syscall
    addiu   $t0, $t0, 4

print_loop_cond:
    bne     $t0, $t1, print_loop

    li      $v0, 4
    la      $a0, EOL
    syscall

    lw      $ra, 28($sp)
    addiu   $sp, $sp, 32
    jr      $ra

#==============================================================================
# ADD YOUR CODE HERE!
#==============================================================================

swap:
# uses two temp registers instead of one so that memory is not used as temp
    lw      $t0, 0($a0) #load value at a0 into t0 (temp regs faster)
    lw      $t1, 0($a1) #load value at a1 into t1 (than memory)
    sw      $t1, 0($a0) #overwrite memory a0 with value in t1(from a1)
    sw      $t0, 0($a1) #overwrite memory a1 with value in t0
    jr      $ra

#stack frame setup:
#save ra
#save a0arr to s2
#save a1heapsize to s1
#save a2i to s0

#set index largest to root t0
#set index left t1
#set index right t2

#compare value at left and largest t1 t0
#set index new largest t3temp
#compare value at right and largest t2 t0
#set index newnew largest t3temp

#check newnew largest index t0 s0
#return if same no change
#swap index largest and root 

#heapify altered subtree
#cleanup stack and return

# void heapify(unsigned* arr, unsigned heap_size, unsigned i)
# NOTE: heapify is recursive and MUST call itself as needed.
heapify:
    addiu   $sp, $sp, -20        # allocate stack frame (four 4byte spaces for ra, s0, s1, s2)
    sw      $ra, 0($sp)          # save return address at sp, incase ra is used for recursion/swap
    sw      $s0, 4($sp)          # save s0 (will hold i) at 1 instruction after sp
    sw      $s1, 8($sp)          # save s1 (will hold heap_size)
    sw      $s2, 12($sp)         # save s2 (will hold arr pointer)
    sw      $s3, 16($sp)         # save s3 (will hold largest)

# now that s0 s1 s2 data is stored, its free for us to use to save our arguments
    move    $s2, $a0             # s2 = arr (array base pointer)
    move    $s1, $a1             # s1 = heap_size
    move    $s0, $a2             # s0 = i (current index)

# the largest index is only relevant within its own frame, can use temporary
    move    $t0, $s0             # t0 = largest = i

#compute left child into t1 by left shifting one bit (x2) and adding 1
    sll     $t1, $s0, 1          # t1 = 2*i
    addiu   $t1, $t1, 1          # t1 = left = 2*i + 1

#block reading into t1 left child if it exceeds the s1 heapsize
#if skip, branch to check_right to skip examining left child
    bge     $t1, $s1, check_right# if left >= heap_size, skip left comparison
    nop                          # delay slot

#proceed to index/lw into value of t1 left child by calculating actual address
#of left child data into t3
    sll     $t3, $t1, 2          # t3 = left * 4; one array index/value is 4 btyes long
    addu    $t3, $s2, $t3        # t3 = &arr[left] add s2, the base address of the array
    lw      $t3, 0($t3)          # t3 = arr[left] overwrite/load the value at t3 into t3

#convert "largest" (index of current root) into its byte address form
    sll     $t4, $t0, 2          # t4 = largest * 4; scale index by 4 to get byte
    addu    $t4, $s2, $t4        # t4 = &arr[largest]; add offset t4, to base of the array s2
    lw      $t4, 0($t4)          # t4 = arr[largest]

#make comparison between t4 "largest" byte address (currently, the root) 
#and t3 left child byte address (not the index form stored in t0/t1)
#ble is the assembly analog to the if statement
    ble     $t3, $t4, check_right# if arr[left] <= arr[largest], skip update
    move    $t0, $t1             # largest = left

check_right:
#now use t2 to hold index value of right child
    sll     $t2, $s0, 1          # t2 = 2*i; double the root index by left shifting once
    addiu   $t2, $t2, 2          # t2 = right = 2*i + 2; and add 2 to get right child

#block reading into t2 right child if t2 exceeds heapsize kept in s1
    bge     $t2, $s1, maybe_done # if right >= heap_size, skip right comparison
    nop                          # delay slot

#proceed to convert indices of right child t2, and t0 largest (could be either root or left)
#t3 is reused to hold the byte address of the right child now (previously we computed the
#left child byte address and put it into t3. it's no longer needed.)
#same process though
    sll     $t3, $t2, 2          # t3 = right * 4
    addu    $t3, $s2, $t3        # t3 = &arr[right]
    lw      $t3, 0($t3)          # t3 = arr[right]

#same process for using t0 "new largest" index to get the "new largest" byte address;
#must be recomputed because "new largest" index could now be either the left child or 
#the root ("old/initial largest")
    sll     $t4, $t0, 2          # t4 = largest * 4
    addu    $t4, $s2, $t4        # t4 = &arr[largest]
    lw      $t4, 0($t4)          # t4 = arr[largest]

#compare new largest with right child, and set the final "largest" index
    ble     $t3, $t4, maybe_done # if arr[right] <= arr[largest], skip update
    move    $t0, $t2             # largest = right

maybe_done:
#check to see if we are in the base case to return
    beq     $t0, $s0, heapify_return # if largest == i, heap property holds
    nop                              # delay slot

#recursion case here, prepare new arguments to perform swap
#convert the index of the current root, stored in s0, into its byte address form and keep in a0
    sll     $a0, $s0, 2          # a0 = i * 4; scale our current index i into bytes
    addu    $a0, $s2, $a0        # a0 = &arr[i]; offset by the address of the array base s2

#convert the index of the largest, stored in t0, into its byte address form and keep in a1
    sll     $a1, $t0, 2          # a1 = largest * 4
    addu    $a1, $s2, $a1        # a1 = &arr[largest]

#call swap with a0 (current root) and a1 (largest, new root) loaded
    move    $s3, $t0      # save largest
    jal     swap                 # swap(arr[i], arr[largest])
    nop                          # delay slot

#now prepare arguments to recursively perform heapify
    move    $a0, $s2             # a0 = arr; feed the byte address of the base of the array
    move    $a1, $s1             # a1 = heap_size; a value in terms of indices, not bytes
    move    $a2, $s3             # a2 = largest; also an index value
    jal     heapify              # recursively heapify affected subtree
    nop

#base case achieved, clean up stack and return back up
heapify_return:
    lw      $ra, 0($sp)          # restore return address
    lw      $s0, 4($sp)          # restore s0
    lw      $s1, 8($sp)          # restore s1
    lw      $s2, 12($sp)         # restore s2
    lw      $s3, 16($sp)         # restore s3
    addiu   $sp, $sp, 20         # deallocate stack frame (move it back the 16 we decremented)
    jr      $ra                  # return to caller



build_max_heap:
#setup stack frame, clearing 12 bytes for ra + 2 stack registers (2 arguments in C signature)
    addiu   $sp, $sp, -16
    sw      $s2, 12($sp) # int value of i (index units) 
    sw      $s1, 8($sp) # int value n (index units)
    sw      $s0, 4($sp) # array base
    sw      $ra, 0($sp) #save return address
    
# now that s0 s1 data is stored, its free for us to use to save our arguments    
    move    $s0, $a0 # array base
    move    $s1, $a1 # int value n (index units)

    #halve by shifting right once
    srl     $s2, $s1, 1
    addiu   $s2, $s2, -1
    
max_inner_loop:
    bltz     $s2, heapify_end #exit loop condition
    nop

    # prep arguments to call heapify again
    move    $a0, $s0 # array base
    move    $a1, $s1 # int n
    move    $a2, $s2 #prep the index from t0 into s2 for heapify to use
    jal     heapify
    nop

    addiu   $s2, $s2, -1 #decrement index i stored in t0
    j       max_inner_loop #branch back again

heapify_end:
    lw      $ra, 0($sp)
    lw      $s0, 4($sp)
    lw      $s1, 8($sp)
    lw      $s2, 12($sp)
    addiu   $sp, $sp, 16
    jr      $ra

# void heapsort(unsigned* arr, unsigned n)
heapsort: 
#setup stack frame, clearing 12 bytes for ra + 3 stack registers (2 arguments in C signature)

    addiu   $sp, $sp, -16
    sw      $s2, 12($sp)# end variable
    sw      $s1, 8($sp) # int n
    sw      $s0, 4($sp) # array base
    sw      $ra, 0($sp) # save return address

    #now that s0 s1 data is stored, its free for us to use to save our arguments  
    move    $s0, $a0 #load in array base address
    move    $s1, $a1 #load in n as max index to govern looping

    #---- if (n <= 1), return ----------------------------------------------
    addiu   $s2 , $s1, -1 #calculate end = n-1 and store in s2. 
    blez    $s2, heapsort_end #before looping, use end (in s2) to check n and return if n<=1
    nop

    #---- call build_max_heap ------------------------------------------------
    move    $a0, $s0   # set a0 as current s0 register for build_max_heap
    move    $a1, $s1   # set a1 as current s1 register for build_max_heap
    jal build_max_heap
    nop

    #???? redundat???
    #load in our "end" integer into s2 for looping arithmetic
    # addiu   $s2, $s1, -1 # set value of end **not really needed

heapsort_inner_loop:
    blez    $s2, heapsort_end # loop conditional check
    nop

    # prep arg registers to call swap
    move    $a0, $s0 # setting &arr[0]

    sll     $t1, $s2, 2 # convert elements to bytes for offset (t1)
    addu    $a1, $s0, $t1 # loading &arr[end]
    jal     swap
    nop

    # decrement end
    addiu   $s2, $s2, -1

    # prep arg registers to call heapify
    move    $a0, $s0
    addiu   $a1, $s2, 1
    move    $a2, $zero
    jal     heapify
    nop

    j       heapsort_inner_loop

heapsort_end:
    lw      $ra, 0($sp)
    lw      $s0, 4($sp)
    lw      $s1, 8($sp)
    lw      $s2, 12($sp)
    addiu   $sp, $sp, 16
    jr      $ra