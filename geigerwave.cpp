#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/*************************************
 * Structure pour le fichier WAVE
 * Récupéré depuis http://www.high-geek.com/archives/169
 */
/*! 
* def unsigned long word 
* Variable de 4 octets 
*/  
typedef unsigned long word;  
/*! 
* def unsigned long dword 
* Variable de 4 octets 
*/  
typedef unsigned long dword;  
      
// Structure  
/** 
* struct WAVE 
* brief Structure d'un fichier WAVE standart 
* Structure codé grace aux structures RIFF, fmt et data d'un fichier audio .wav 
* 
* 
*ingroup wave 
*/  
      
typedef struct WAVE
{  
      
	/** 
	*struct RIFF 
	*brief Description principal du fichier 
	* "RIFF" (chunk descriptor) structure décrivant la nature du fichier. 
	*/  
	struct RIFF
	{  
		char ChunkID[4];    // contient les lettres "RIFF" pour indiquer que le fichier est codé selon la norme RIFF  
		word ChunkSize;        // taille du fichier entier en octets (sans compter les 8 octets de ce champ (4o) et le champ précédent CunkID (4o)  
		char Format[4];        // correspond au format du fichier donc ici, contient les lettres "WAVE" car fichier est au format wave  
	} RIFF;  
      
	/** 
	*struct fmt 
	*brief Sprécifications du format audio 
	* "fmt " (sub-chunk) structure décrivant le format des données audio 
	*/  
	struct fmt
	{  
		char Subchunk1ID[4];    // contient les lettres "fmt " pour indiquer les données à suivre décrivent le format des données audio  
		dword Subchunk1Size;    // taille en octet des données à suivre (qui suivent cette variable) 16 Pour un fichier PCM  
		short AudioFormat;        // format de compression (une valeur autre que 1 indique une compression)  
		short NumChannels;        // nombre de canaux: Mono = 1, Stereo = 2, etc..  
		dword SampleRate;        // fréquence d'échantillonage, ex 44100, 44800 (nombre d'échantillons par secondes)  
		dword ByteRate;            // nombre d'octects par secondes  
		short Blockalign;        // nombre d'octects pour coder un échantillon  
		short BitsPerSample;    // nombre de bits pour coder un échantillon  
	} fmt;  
      
	/** 
	* 
	*struct data 
	*brief Données du fichier audio 
	* "data" (sub-chunk) contient les données audio, les échantillons et le nombre d'octets qu'ils représentent. 
	*/  
	struct data
	{  
		char Subchunk2ID[4];    // contient les lettres "data" pour indiquer que les données à suivre sont les données audio (les échantillons et)  
		dword Subchunk2Size;    // taille des données audio (nombre total d'octets codant les données audio)  
	} data;  
} WAVE;  

#if defined WIN32 && defined _MSC_VER
// définis par le compilateur Microsoft
typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
typedef char int8_t;
typedef short int16_t;
typedef long int32_t;
typedef longlong int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned longlong uint64_t;
#endif

#define INT8_MAX	(127)
#define INT8_MIN	(-128)

/* More or less an extremely crude derivative function... */
static int8_t
evolution(int32_t value1, int32_t value2, int threshold)
{
	int32_t ret = (value2 - value1) / threshold;
	if (ret > INT8_MAX) {
		ret = INT8_MAX;
	}
	if (ret < INT8_MIN) {
		ret = INT8_MIN;
	}
	return (int8_t) ret;
}


/* Naive peak detection (both positive and negative ones).
   Note that, as this function is called for every value,
   the call to evolution(a, b, th) is the same as the call
   to evolution(b, c, th) of the previous call to peakp().
   This could hence be optimized if needed.
 */
static bool
peakp(int16_t a, int16_t b, int16_t c, int th)
{
	int8_t le = evolution(a, b, th);
	int8_t ce = evolution(b, c, th);

	/* Positive peaks */
	if (le >= 0 && ce < 0 && b > th)
		return true;

#if 0
	/* Negative peaks */
	if (le <= 0 && ce > 0 && b < -th)
		return true;
#endif
	return false;
}


/* This structure will hold data to and from the counting algorithm.
An instance of this structure is used with the callback function
that process the audio input. */
struct countdata {
	double start_time;
	uint64_t count;
	int threshold; // detection threshold to filter noise
	int16_t last_values[2]; /* remember the last values between calls to the
					callback fct */
	double last_spl_time;   /* timestamp for the last sample processed */
	uint64_t sample_number; /* count the number of samples (= time) */
};
static const struct countdata init_cd = {0.0, 0, 0, {0,0}, 0.0, 0};


static bool ProcessFile(const char *zFilename, const int nThreshold);

int main(int argc, char *argv[])
{
	if (argc<2)
	{
		fprintf(stderr,"usage : %s <fichier wave>\n",argv[0]);
		return EXIT_FAILURE;
	}
	// TODO : permettre de configurer le seuil à la ligne de commande
	const int nThreshold=5;
	ProcessFile(argv[1],nThreshold);

	return EXIT_SUCCESS;
}

static bool ReadWaveFile(const char *zFilename,
						 int16_t *&pData,
						 int32_t &nSampleCount,
						 int32_t &nSampleRate)
{
	FILE *file=NULL;
	int nError=0;
	
	// dummy loop
	do
	{
		// on ouvre le fichier
		file=fopen(zFilename,"rb");
		if (!file)
		{
			nError=errno;
			fprintf(stderr,"Le fichier \"%s\" n'a pas pu etre ouvert.\n",
				zFilename);
			break;
		}

		WAVE Header;
		// on lit l'entête
		if (fread(&Header,sizeof(Header),1,file)!=1)
		{
			nError=errno;
			fprintf(stderr,"Erreur pendant la lecture de l'entete du fichier \"%s\".",
				zFilename);
			break;
		}

		nSampleCount=(int)(Header.data.Subchunk2Size + 1) / (int)Header.fmt.Blockalign;
		pData=(short *)malloc(nSampleCount*sizeof(short));
		if (!pData)
		{
			nError=errno;
			fprintf(stderr,"Erreur pendant l'allocation du buffer (%d echantillons).",
				nSampleCount);
			break;
		}
		nSampleRate=Header.fmt.SampleRate;

		//remplissage du tableau d'échantillons data[] de la structure WAVE  
		fread(pData, sizeof(short), nSampleCount, file);  
		// tout est ok!
	} while (false);

	// si erreur, on laisse un petit message
	if (nError)
	{
		fprintf(stderr,"Erreur %d (%s)\n",
			nError,strerror(nError));
	}

	// on nettoie
	if (file)
	{
		// on ferme le fichier
		fclose(file);
	}

	return nError==0;
}


static bool ProcessData(struct countdata *data,
						const int16_t *pData,
						const int32_t nSampleCount,
						const int32_t nSampleRate)
{
	int16_t prev0 = data->last_values[0];
	int16_t prev1 = data->last_values[1];
	double time = data->start_time;
	for (int i = 0; i < nSampleCount; i++)
	{
	//	fprintf(stderr, "%d ", in[i]);
		/* The actual sample rate seems to be only an approximation of
		   SAMPLE_RATE, hence 'time + (double) i / SAMPLE_RATE' is only
		   an estimation of the sampling time. */
		if (peakp(prev0, prev1, pData[i], data->threshold))
		{
			data->count++;
			printf("%.4f\t%6d\n", time + (double) i / nSampleRate,
			       prev1);
		}
		prev0 = prev1;
		prev1 = pData[i];
		data->sample_number++;
	}
	data->last_values[0] = prev0;
	data->last_values[1] = prev1;
	data->last_spl_time = time + (nSampleCount - 1.0) / nSampleRate;

	return true;
}


static bool ProcessFile(const char *zFilename, const int nThreshold)
{
	int16_t *pData;
	int32_t nSampleCount;
	int32_t nSampleRate;

	if (!ReadWaveFile(zFilename,pData,nSampleCount,nSampleRate))
	{
		return false;
	}
	
	struct countdata data;
	data=init_cd;
	data.threshold=nThreshold;

	ProcessData(&data,pData,nSampleCount,nSampleRate);

	free(pData);
	return true;
}