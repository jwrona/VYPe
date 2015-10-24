/* Program 2: Vypocet faktorialu (rekurzivne) */

int factorial (int);
int factorial (int n)
{
	int decremented_n, temp_result;
	if (n < 2)
	{
		return 1;
	}
	else
	{
		decremented_n = n - 1;
		temp_result = factorial(decremented_n);
	}
	return (int)n * temp_result;
} // end of factorial

int main(void)
{
	int a; int vysl;
	print("Zadejte cislo pro vypocet faktorialu");
	a = read_int();
	if (a < 0)
	{
		print("\nFaktorial nelze spocitat\n");
	}
	else
	{
		print("\nVysledek je:", factorial(a), "\n");
	}
}
