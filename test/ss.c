#include <stdio.h>
long int a; //for address 
   long int i;
   int j,k,l,m,n;
   long int page;
   long int b[30000][2]={0};
   int u;
main()
{
  FILE *fp;
   fp = fopen("D:\\0Fall14\\ca\\hw4\\sb\\pinatrace.txt", "r");
   
  //long int a; //for address 
   //long int i;
   //int j,k,l,m,n;
   //long int page;
   //long int b[30000][2]={0};
   //int u;
    for(i=0;!feof(fp);i++)
   { 
        fscanf_s(fp,"%*lx: %*c %lx",&a); //store the temp. address into a 
        //a=a/32; //cache line size is 32 byte
        a=a/128; //cache line size is 128 byte
        u=0; //counter
        for(j=0;j<30000;j++)
        {
           if(a==b[j][0]) 
           {
            //k=j;
            //b[k][0]=a;
            //b[k][1]++;
            break;   
           } 
           else 
           u++;
        }
        if(u==30000)
        {
            for(l=0;l<30000;l++)
            {
                if(b[l][1]==0)
                {
                    b[l][0]=a;
                    n++;
                    break;
                }
            }
        }
    } 
    //for(m=0;m<100;m++)
    //{
        printf("There are %d unique cache blocks has been accessed",n); 
    //}
        
    fclose(fp);
	fp=NULL;
	
}
