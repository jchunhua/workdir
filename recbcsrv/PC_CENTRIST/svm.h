#ifndef _LIBSVM_H
#define _LIBSVM_H

#define LIBSVM_VERSION 288
#include "param.h"

typedef short svm_node;

//
// svm_model
//
typedef struct svm_model
{
	double gamma;	// parameter
	int nr_class;		// number of classes, = 2 in regression/one class svm
	int l;			// total #SV
	double *rho;		// constants in decision functions (rho[k*(k-1)/2])
	int *label;		// label of each class (label[k])
	int *nSV;		// number of SVs for each class (nSV[k])
	svm_node **SV;		// SVs (SV[l])
	double **sv_coef;	// coefficients for SVs in decision functions (sv_coef[k-1][l])
}svm_model;

extern void svm_destroy_model(struct svm_model *model);
extern struct svm_model *svm_load_model(const char *model_file_name);
extern double svm_predict(const struct svm_model *model, const svm_node *x);
extern void loadFeature( const char *FeatureName);

extern const int m_scale_ratio;
#endif /* _LIBSVM_H */