#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <omp.h>
#include <vector>
#include <algorithm> // std::sort を使うために追加
#include <utility>   // std::pair を使うために追加

int number_bacteria;
char** bacteria_name;
long M, M1, M2;
short code[27] = { 0, 2, 1, 2, 3, 4, 5, 6, 7, -1, 8, 9, 10, 11, -1, 12, 13, 14, 15, 16, 1, 17, 18, 5, 19, 3};
#define encode(ch)		code[ch-'A']
#define LEN				6
#define AA_NUMBER		20
#define	EPSILON			1e-010

void Init()
{
	M2 = 1;
	for (int i=0; i<LEN-2; i++)	// M2 = AA_NUMBER ^ (LEN-2);
		M2 *= AA_NUMBER; 
	M1 = M2 * AA_NUMBER;		// M1 = AA_NUMBER ^ (LEN-1);
	M  = M1 *AA_NUMBER;			// M  = AA_NUMBER ^ (LEN);
}

class Bacteria
{
private:
	long* vector;
	long* second;
	long one_l[AA_NUMBER];
	long indexs;
	long total;
	long total_l;
	long complement;

	void InitVectors()
	{
		vector = new long [M];
		second = new long [M1];
		memset(vector, 0, M * sizeof(long));
		memset(second, 0, M1 * sizeof(long));
		memset(one_l, 0, AA_NUMBER * sizeof(long));
		total = 0;
		total_l = 0;
		complement = 0;
	}

	void init_buffer(char* buffer)
	{
		complement++;
		indexs = 0;
		for (int i=0; i<LEN-1; i++)
		{
			short enc = encode(buffer[i]);
			one_l[enc]++;
			total_l++;
			indexs = indexs * AA_NUMBER + enc;
		}
		second[indexs]++;
	}

	void cont_buffer(char ch)
	{
		short enc = encode(ch);
		one_l[enc]++;
		total_l++;
		long index = indexs * AA_NUMBER + enc;
		vector[index]++;
		total++;
		indexs = (indexs % M2) * AA_NUMBER + enc;
		second[indexs]++;
	}

public:
	long count;
	double* tv;
	long *ti;
	double l2norm;

	static const size_t IO_CHUNK = 1u << 20;

	Bacteria(char* filename)
	{
		FILE* bacteria_file = fopen(filename, "rb");

		if (bacteria_file == NULL)
		{
			fprintf(stderr, "Error: failed to open file %s\n", filename);
			exit(1);
		}

		setvbuf(bacteria_file, NULL, _IOFBF, IO_CHUNK);

		InitVectors();

		std::vector<char> buf(IO_CHUNK);
		enum State { IN_HEADER, IN_SEQ } st = IN_SEQ;
		char win[LEN-1]; int wlen = 0;



		size_t n;
		while ((n = fread(buf.data(), 1, buf.size(), bacteria_file)) > 0) {
			for (size_t k = 0; k < n; ++k) {
				char ch = buf[k];
				if (ch == '>') { st = IN_HEADER; wlen = 0; continue; }
				if (st == IN_HEADER) { if (ch == '\n') st = IN_SEQ; continue; }
				if (ch == '\n' || ch =='\r' || ch == ' ' || ch == '\t') continue; // 配列行の改行は捨てる
				if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
				int idx = (ch >= 'A' && ch <= 'Z') ? (ch - 'A') : -1;
				short enc = (idx >= 0 && idx < 27) ? code[idx] : (short)-1;
				if (enc < 0) {
					// ここで k-mer は途切れた扱いにする：再プライムが必要
					wlen = 0;
					continue;
				}
				if (wlen < (LEN - 1)) {
					win[wlen++] = ch;
					if (wlen == (LEN - 1)) init_buffer(win);
				} else {
					cont_buffer(ch);
				}
			}
		}
		fclose(bacteria_file);

		long total_plus_complement = total + complement;
		double total_div_2 = total * 0.5;

		double one_l_div_total[AA_NUMBER];
		for (int i=0; i<AA_NUMBER; i++)
			one_l_div_total[i] = (double)one_l[i] / total_l;

		double* second_div_total = new double[M1];
		for (long i=0; i<M1; i++)
			second_div_total[i] = (double)second[i] / total_plus_complement;

		count = 0;

		std::vector<std::vector<long>>   tls_idx(omp_get_max_threads());
		std::vector<std::vector<double>> tls_val(omp_get_max_threads());
#pragma omp parallel
		{
			int tid = omp_get_thread_num();
			auto &Lidx = tls_idx[tid];
			auto &Lval = tls_val[tid];

			// 事前にある程度リザーブ（任意。空間が許すなら効果大）
			// Lidx.reserve(1<<20); Lval.reserve(1<<20);

		#pragma omp for
				for (long i = 0; i < M; ++i) {
					long idx_div_aa = i / AA_NUMBER;
					int  aa0        = i % AA_NUMBER;
					long idx_div_M1 = i / M1;
					long idx_mod_M1 = i % M1;

					double p1 = second_div_total[idx_div_aa];
					double p2 = one_l_div_total[aa0];
					double p3 = second_div_total[idx_mod_M1];
					double p4 = one_l_div_total[idx_div_M1];
					double stochastic = (p1*p2 + p3*p4) * total_div_2;

					if (stochastic > EPSILON) {
						double val = (vector[i] - stochastic) / stochastic;
						if (val != 0.0) {    // ほぼ常に真だが、ゼロ除外はそのまま
							Lidx.push_back(i);
							Lval.push_back(val);
						}
					}
				}

		}

		delete[] second_div_total;
		delete[] vector;
		delete[] second;

		size_t nnz = 0;
		for (auto &v : tls_idx) nnz += v.size();
		count = (long)nnz;

		tv = new double[count];
		ti = new long[count];

		size_t off = 0;
		for (size_t t = 0; t < tls_idx.size(); ++t) {
			auto n = tls_idx[t].size();
			if (n == 0) continue;
			memcpy(ti + off, tls_idx[t].data(), n * sizeof(long));
			memcpy(tv + off, tls_val[t].data(), n * sizeof(double));
			off += n;
		}
		// 1. tiとtvをペアにする
		std::vector<std::pair<long, double>> sorted_pairs(count);
		for (long i = 0; i < count; i++) {
			sorted_pairs[i] = {ti[i], tv[i]};
		}

		// 2. ペアをインデックス(pair.first)基準でソートする
		std::sort(sorted_pairs.begin(), sorted_pairs.end());

		// 3. ソートされた結果を元の配列に戻す
		for (long i = 0; i < count; i++) {
			ti[i] = sorted_pairs[i].first;
			tv[i] = sorted_pairs[i].second;
		}


		double sum_sq = 0.0;
		for (long k = 0; k < this->count; k++)
		{
			sum_sq += this->tv[k] * this->tv[k];
		}
		this->l2norm = sqrt(sum_sq);
	}
	~Bacteria() {
		delete[] tv;
		delete[] ti;
	}
};

void ReadInputFile(const char* input_name)
{
	FILE* input_file = fopen(input_name, "r");

	if (input_file == NULL)
	{
		fprintf(stderr, "Error: failed to open file %s (Hint: check your working directory)\n", input_name);
		exit(1);
	}

	if (fscanf(input_file, "%d", &number_bacteria) != 1)
	{
		fprintf(stderr, "Error: invalid number of bacteria in file %s\n", input_name);
		exit(1);
	}
	bacteria_name = new char*[number_bacteria];

	for(long i=0;i<number_bacteria;i++)
	{
		char name[10];
		if (fscanf(input_file, "%9s", name) != 1)
		{
			fprintf(stderr, "Error: invalid bacteria name in file %s\n", input_name);
			exit(1);
		}
		bacteria_name[i] = new char[20];
		snprintf(bacteria_name[i], 20, "data/%s.faa", name);
	}
	fclose(input_file);
}

double CompareBacteria(Bacteria* b1, Bacteria* b2)
{
	double dot_product = 0.0;
	long p1 = 0;
	long p2 = 0;
	while (p1 < b1->count && p2 < b2->count)
	{
		long n1 = b1->ti[p1];
		long n2 = b2->ti[p2];
		if (n1 < n2)
		{
			p1++;
		}
		else if (n2 < n1)
		{
			p2++;
		}
		else
		{
			dot_product += b1->tv[p1++] * b2->tv[p2++];
		}
	}

	if (b1->l2norm < EPSILON || b2->l2norm < EPSILON)
	{
		return 0.0;
	}

	return dot_product / (b1->l2norm * b2->l2norm);
}

void CompareAllBacteria()
{
	Bacteria** b = new Bacteria*[number_bacteria];
	const int K = 2;
	static int io_token[K] = {0};

	#pragma omp parallel
	#pragma omp single
	{
		for(int i=0; i<number_bacteria; i++)
		{
#pragma omp task firstprivate(i) depend(inout: io_token[i%K])
			{
				b[i] = new Bacteria(bacteria_name[i]);
#pragma omp critical
				printf("load %d of %d\n", i+1, number_bacteria);
			}
		}
#pragma omp taskwait
	}
	#pragma omp parallel for schedule(dynamic)
	for(int i=0; i<number_bacteria; i++)
	{
		for(int j=i+1; j<number_bacteria; j++)
			{
				double correlation = CompareBacteria(b[i], b[j]);
				#pragma omp critical
				{
					printf("%2d %2d -> %.20lf\n", i, j, correlation);
				}
			}
		}
	for (int i = 0; i < number_bacteria; i++) {
		delete b[i];
	}
	delete[] b;
}

int main(int argc,char * argv[])
{
	printf("threads: %d\n", omp_get_max_threads());
	time_t t1 = time(NULL);

	Init();
	ReadInputFile("list.txt");
	CompareAllBacteria();

	for (int i = 0; i < number_bacteria; i++) {
		delete[] bacteria_name[i];
	}
	delete[] bacteria_name;

	time_t t2 = time(NULL);
	printf("time elapsed: %lld seconds\n", (long long)(t2 - t1));
	return 0;
}