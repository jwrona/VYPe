/* Program 1: Vypocet faktorialu (iterativne) */

int main(void) { // Hlavni telo programu
	int a, vysl;

	print("Zadejte cislo pro vypocet faktorialu");
	a = read_int();
	if (a < 0) {
		print("\nFaktorial nelze spocitat\n"); }
	else
	{
		vysl = 1;
		while (a > 0)
		{
			vysl = vysl * a;
			a = a - 1;
		} // endwhile
		print("\nVysledek je: ", vysl, "\n");
	} // endif
} // main
