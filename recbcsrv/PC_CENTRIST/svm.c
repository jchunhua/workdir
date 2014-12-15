#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <float.h>
#include <string.h>
#include <stdarg.h>
#include "svm.h"
#include "param.h"

#define Malloc(type,n) (type *)malloc((n)*sizeof(type))

const int m_scale_ratio = SCALE*SCALE;

extern int *Feature_Min;
extern int *Feature_Max;

void loadFeature(const char *feature_file_name)
{
    int line = 0;
    int i = 0;
    FILE *fp = fopen(feature_file_name,"rb");
	if(fp==NULL)
    {
        fprintf(stderr, "Error: Invalid feature range file-->%s\n", feature_file_name);
        exit(-1);
    }

    Feature_Min = Malloc(int, DIM);
    Feature_Max = Malloc(int, DIM);
    
    for(i = 0; i < DIM;)
    {
        fscanf(fp, "%d %d %d", &line, &Feature_Min[i], &Feature_Max[i]);

        if(line != ++i)
        {
            fprintf(stderr, "Missing %d dim feature range\n", i);
            exit(-1);
        }
    }

    fclose(fp);
}


int k_function(const svm_node *x, const svm_node *y)
{
	int sum = 0;
	int i = 0;
	while(i < DIM)
	{
		int d = x[i] - y[i];
		sum += d*d;
		++i;
	}
	return sum;
}

void svm_predict_values(const svm_model *model, const svm_node *x, double* dec_values)
{
	int i;
	int nr_class = model->nr_class;
	int l = model->l;
		
	double *kvalue = Malloc(double,l);
	for(i=0;i<l;i++)
		// kvalue[i] = 0;
		kvalue[i] = exp(-model->gamma*k_function(x,model->SV[i])/m_scale_ratio);

	int *start = Malloc(int,nr_class);
	start[0] = 0;
	for(i=1;i<nr_class;i++)
		start[i] = start[i-1]+model->nSV[i-1];

	int p=0, j;
	for(i=0;i<nr_class;i++)
		for( j=i+1;j<nr_class;j++)
		{
			double sum = 0;
			int si = start[i];
			int sj = start[j];
			int ci = model->nSV[i];
			int cj = model->nSV[j];
				
			int k;
			double *coef1 = model->sv_coef[j-1];
			double *coef2 = model->sv_coef[i];
			for(k=0;k<ci;k++)
				sum += coef1[si+k] * kvalue[si+k];
			for(k=0;k<cj;k++)
				sum += coef2[sj+k] * kvalue[sj+k];
			sum -= model->rho[p];
			dec_values[p] = sum;
			p++;
		}

	free(kvalue);
	free(start);
	
}

double svm_predict(const svm_model *model, const svm_node *x)
{
	int i;
	int nr_class = model->nr_class;
	double *dec_values = Malloc(double, nr_class*(nr_class-1)/2);
	svm_predict_values(model, x, dec_values);

	int *vote = Malloc(int,nr_class);
	for(i=0;i<nr_class;i++)
		vote[i] = 0;
	int pos=0, j;
	for(i=0;i<nr_class;i++)
		for( j=i+1;j<nr_class;j++)
		{
			if(dec_values[pos++] > 0)
				++vote[i];
			else
				++vote[j];
		}

	int vote_max_idx = 0;
	for(i=1;i<nr_class;i++)
		if(vote[i] > vote[vote_max_idx])
			vote_max_idx = i;
	free(vote);
	free(dec_values);
	return model->label[vote_max_idx];
	
}

svm_model *svm_load_model(const char *model_file_name)
{
	FILE *fp = fopen(model_file_name,"rb");
	if(fp==NULL) return NULL;
	
	// read parameters
	svm_model *model = Malloc(svm_model,1);
	model->rho = NULL;
	model->label = NULL;
	model->nSV = NULL;

	int i;
	char cmd[81];
	while(1)
	{
		fscanf(fp,"%80s",cmd);
		if(strcmp(cmd,"gamma")==0)
			fscanf(fp,"%lf",&model->gamma);
		else if(strcmp(cmd,"nr_class")==0)
			fscanf(fp,"%d",&model->nr_class);
		else if(strcmp(cmd,"total_sv")==0)
			fscanf(fp,"%d",&model->l);
		else if(strcmp(cmd,"rho")==0)
		{
			int n = model->nr_class * (model->nr_class-1)/2;
			model->rho = Malloc(double,n);
			for( i=0;i<n;i++)
				fscanf(fp,"%lf",&model->rho[i]);
		}
		else if(strcmp(cmd,"label")==0)
		{
			int n = model->nr_class;
			model->label = Malloc(int,n);
			for( i=0;i<n;i++)
				fscanf(fp,"%d",&model->label[i]);
		}
		else if(strcmp(cmd,"nr_sv")==0)
		{
			int n = model->nr_class;
			model->nSV = Malloc(int,n);
			for( i=0;i<n;i++)
				fscanf(fp,"%d",&model->nSV[i]);
		}
		else if(strcmp(cmd,"SV")==0)
		{
			while(1)
			{
				int c = getc(fp);
				if(c==EOF || c=='\n') break;	
			}
			break;
		}
		else
		{
			fprintf(stderr,"unknown text in model file: [%s]\n",cmd);
			free(model->rho);
			free(model->label);
			free(model->nSV);
			free(model);
			return NULL;
		}
	}

	// read sv_coef and SV

	int elements = 0;
	long pos = ftell(fp);

	while(1)
	{
		int c = fgetc(fp);
		switch(c)
		{
			case '\n':
				// count the '-1' element
			case ':':
				++elements;
				break;
			case EOF:
				goto out;
			default:
				;
		}
	}
out:
	fseek(fp,pos,SEEK_SET);

	int m = model->nr_class - 1;
	int l = model->l;
	model->sv_coef = Malloc(double *,m);

	for(i=0;i<m;i++)
		model->sv_coef[i] = Malloc(double,l);
	model->SV = Malloc(svm_node*,l);
	svm_node *x_space=NULL;
	if(l>0) x_space = Malloc(svm_node,elements);

	int j=0, k;
	for(i=0;i<l;i++)
	{
		model->SV[i] = &x_space[j];
		for( k=0;k<m;k++)
			fscanf(fp,"%lf",&model->sv_coef[k][i]);
		while(1)
		{
			int c;
			do {
				c = getc(fp);
				if(c=='\n') goto out2;
			} while(isspace(c));
			ungetc(c,fp);
			fscanf(fp,":%hd", &(x_space[j]));
			++j;
		}	
out2:
		;//x_space[j++].index = -1;
	}
	if (ferror(fp) != 0 || fclose(fp) != 0) return NULL;

	return model;
}

void svm_destroy_model(svm_model* model)
{
	int i;
	if(model->l > 0)
		free((void *)(model->SV[0]));
	for( i=0;i<model->nr_class-1;i++)
		free(model->sv_coef[i]);
	free(model->SV);
	free(model->sv_coef);
	free(model->rho);
	free(model->label);
	free(model->nSV);
	free(model);

	printf("=======free model  done========\n");
}
