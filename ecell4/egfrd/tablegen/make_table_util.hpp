#ifndef __MAKE_TABLE_UTIL_HPP
#define __MAKE_TABLE_UTIL_HPP

#include <string>
#include <fstream>

typedef std::vector<double> table_type;
typedef std::vector<std::pair<double, double> > valdot_type;

void write_header(std::ofstream& ofs,
        const std::string& header_name, const std::string& ns_name)
{
    const std::string header = "\
#ifndef " + header_name + "\n\
#define " + header_name + "\n\
\n\
/* Auto-generated by a program.  Do not edit. */\n\
\n\
namespace " + ns_name + "\n\
{\n\
\n\
struct Table\n\
{\n\
    const unsigned int N;\n\
    const double x_start;\n\
    const double delta_x;\n\
    const double* const y;\n\
};";
    ofs << header << std::endl;
}

void write_footer(std::ofstream& ofs,
        const std::string& header_name, const std::string& ns_name)
{
    const std::string footer = "\
} //namespace " + ns_name + "\n\
\n\
#endif /* " + header_name + " */\n\
";
    ofs << footer << std::endl;
}

void write_table_array(std::ofstream& ofs, const std::string name,
        const int minn, const int maxn)
{
    ofs << "static unsigned int " << name << "_min(" << minn << ");\n"
        << "static unsigned int " << name << "_max(" << maxn << ");\n"
        << "static const Table* " << name << "[" << maxn+1 << "] =\n{\n";
    for (int n(0); n < minn; ++n)
        ofs << "    0,\n";
    for (int n(minn); n <= maxn; ++n)
        ofs << "    &" << name << n << ",\n";
    ofs << "};\n" << std::endl;
}

void write_arrays(std::ofstream& ofs, const std::string name,
        const valdot_type table)
{
    const int N(table.size());

    ofs << "\nstatic const double " << name << "[" << 2*N << " + 1] =\n{\n";
    if (N >=1)
    {
        valdot_type::const_iterator itr(table.begin());
        ofs << "    "
            << std::scientific << std::setprecision(18) << (*itr).first << ", "
            << std::scientific << std::setprecision(18) << (*itr).second;

        while((++itr) != table.end())
            ofs << ",\n    "
                << std::scientific << std::setprecision(18) << (*itr).first << ", "
                << std::scientific << std::setprecision(18) << (*itr).second;
    }
    ofs << "};" << std::endl;
}

void write_table(std::ofstream& ofs, const std::string& name,
        const int N, const double x_start, const double delta_x)
{
    ofs << "\nstatic const Table " << name << " = { "
        << N << ", "
        << std::fixed << std::setprecision(18) << x_start << ", "
        << std::fixed << std::setprecision(18) << delta_x << ", "
        << name << "_f };\n" << std::endl;
}

#endif
