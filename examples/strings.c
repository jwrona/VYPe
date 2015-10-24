/* Program 3: Prace s retezci a vestavenymi funkcemi */

int main(void) { //Hlavni telo programu
        string str1, str2;
        int p; char zero;

        str1 = "Toto je nejaky text";
        str2 = strcat(str1, ", ktery jeste trochu obohatime");
        print(str1, '\n', str2, "\n");

        str1 = read_string();
        while ((int)(get_at(str1, p)) != 0)
        {
                p = p + 1;
        }
        print("\nDelka retezce \"", str1, "\", je ", p, " znaku.\n");
}
