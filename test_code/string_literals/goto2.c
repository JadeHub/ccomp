int main(void)
{
    int i = 0;	
    // goto can be used to leave a multi-level loop easily
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
            i++;
            if (x + y >= 3) goto endloop2;
        }
    }
endloop: endloop2 :;
    return i;
}
