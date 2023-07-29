#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>

#include <curl/curl.h>
#include <libxml/HTMLparser.h>

#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace request
{
    static int on_download_data(char *data, size_t size, size_t nmemb, std::string *buffer)
    {
        if (buffer == nullptr)
        {
            return 0;
        }
        const auto rsize = size * nmemb;
        buffer->append(data, rsize);
        return rsize;
    }

    static char error_buffer[CURL_ERROR_SIZE];

    void is_valid(const CURLcode code)
    {
        if (code != CURLE_OK)
        {
            throw std::runtime_error("curl code: " + std::to_string(static_cast<int>(code)));
        }
    }

    void init()
    {
        static bool init = true;
        if (init)
        {
            init = false;
            is_valid(curl_global_init(CURL_GLOBAL_DEFAULT));
        }
    }

    class Context
    {
        CURL *m_context       = nullptr;
        curl_slist *m_headers = nullptr;

    public:
        Context()
        {
            m_context = curl_easy_init();
            set_op(CURLOPT_SSL_VERIFYPEER, 0L);
            set_op(CURLOPT_SSL_VERIFYHOST, 0L);
        }

        ~Context()
        {
            if (m_headers)
            {
                curl_slist_free_all(m_headers);
            }
            curl_easy_cleanup(m_context);
        }

        template <typename OP, typename T>
        void set_op(const OP op, const T data)
        {
            is_valid(curl_easy_setopt(m_context, op, data));
        }

        void add_header(const std::string_view &value)
        {
            m_headers = curl_slist_append(m_headers, value.data());
        }

        long perform()
        {
            long result = 0;
            if (m_headers)
            {
                is_valid(curl_easy_setopt(m_context, CURLOPT_HTTPHEADER, m_headers));
            }
            is_valid(curl_easy_perform(m_context));
            is_valid(curl_easy_getinfo(m_context, CURLINFO_RESPONSE_CODE, &result));
            return result;
        }
    };

    std::map<std::string, std::string> s_cache;

    std::optional<std::string> get(const std::string &address)
    {
        if (s_cache.count(address) > 0)
        {
            return s_cache[address];
        }

        try
        {
            init();
            std::string result;
            {
                Context context;
                context.set_op(CURLOPT_VERBOSE, 0L);
                context.set_op(CURLOPT_HTTPGET, 1L);
                context.set_op(CURLOPT_ERRORBUFFER, error_buffer);
                context.set_op(CURLOPT_URL, address.c_str());
                context.set_op(CURLOPT_FOLLOWLOCATION, 1L);
                context.set_op(CURLOPT_WRITEFUNCTION, request::on_download_data);
                context.set_op(CURLOPT_WRITEDATA, &result);
                context.set_op(
                    CURLOPT_USERAGENT,
                    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                    "(KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36");

                if (context.perform() != 200)
                {
                    std::cerr << "response status: [" << context.perform() << "]";
                    return std::nullopt;
                }
            }

            if (result.empty())
            {
                std::cerr << "empty response";
                return std::nullopt;
            }

            s_cache[address] = result;
            return result;
        }
        catch (const std::exception &e)
        {
            std::cerr << "error message: [" << e.what() << "]";
            return std::nullopt;
        }
        return std::nullopt;
    }
} // namespace request

namespace html
{
    namespace impl
    {
        struct Dividends
        {
            Dividends() {}

            bool m_in_earning_section = false;
            bool m_in_input           = false;
            bool m_in_results         = false;

            boost::json::stream_parser json_parser;

            void start_element(const std::string_view &name)
            {
                if (m_in_input)
                {
                    return;
                }

                if (name == "input")
                {
                    m_in_input = true;
                }
            }

            void element_attribute(const std::string_view &name, const std::string_view &value)
            {
                if (name == "id")
                {
                    if (value == "earning-section")
                    {
                        m_in_earning_section = true;
                    }
                    else if (value == "results")
                    {
                        m_in_results = true;
                    }
                }
                else if (m_in_earning_section && m_in_input && m_in_results)
                {
                    if (name == "value")
                    {
                        boost::system::error_code ec;
                        json_parser.write(value, ec);
                        if (ec)
                        {
                            throw std::runtime_error(ec.message());
                        }
                    }
                }
            }

            void end_element(const std::string_view &name)
            {
                if (!m_in_input)
                {
                    return;
                }

                if (name == "input")
                {
                    m_in_results         = false;
                    m_in_input           = false;
                    m_in_earning_section = false;
                }
            }

            void data(const std::string_view &data)
            {
                (void)(data);
            }
        };

        static void start_element(void *octx, const xmlChar *name, const xmlChar **attributes)
        {
            static_cast<Dividends *>(octx)->start_element(reinterpret_cast<const char *>(name));
            auto att = attributes;
            while (att && *att)
            {
                const std::string_view name = reinterpret_cast<const char *>(*att);
                att++;

                std::string_view value;
                if (att && *att)
                {
                    value = std::string_view(reinterpret_cast<const char *>(*att));
                    att++;
                }

                static_cast<Dividends *>(octx)->element_attribute(name, value);
            }
        }

        static void end_element(void *octx, const xmlChar *name)
        {
            static_cast<Dividends *>(octx)->end_element(reinterpret_cast<const char *>(name));
        }

        static void characters(void *octx, const xmlChar *chars, int length)
        {
            static_cast<Dividends *>(octx)->data(std::string_view(reinterpret_cast<const char *>(chars), length));
        }

        static void cdata(void *octx, const xmlChar *chars, int length)
        {
            static_cast<Dividends *>(octx)->data(std::string_view(reinterpret_cast<const char *>(chars), length));
        }

        static htmlSAXHandler sax_handler = {NULL, NULL, NULL, NULL, NULL, NULL,          NULL,        NULL,  NULL,
                                             NULL, NULL, NULL, NULL, NULL, start_element, end_element, NULL,  characters,
                                             NULL, NULL, NULL, NULL, NULL, NULL,          NULL,        cdata, NULL};
    } // namespace impl

    namespace parsers
    {
        class Dividend
        {
        public:
            enum class Type
            {
                jcp,
                dividend
            };

            explicit Dividend(const boost::json::value &data)
            {
                m_date_pay = boost::json::value_to<std::string>(data.at("pd"));
                m_date_ex  = boost::json::value_to<std::string>(data.at("ed"));
                m_value    = std::stold(boost::replace_all_copy(boost::json::value_to<std::string>(data.at("sv")), ",", "."));

                {
                    const auto type = boost::json::value_to<std::string>(data.at("et"));
                    if (type == "JCP")
                    {
                        m_type = Type::jcp;
                    }
                    else if (type == "Dividendo")
                    {
                        m_type = Type::dividend;
                    }
                    else
                    {
                        throw std::runtime_error("dividend type not supported");
                    }
                }
            }

            static std::vector<Dividend> load(const boost::json::value &data)
            {
                if (!data.is_array())
                {
                    return {};
                }

                std::vector<Dividend> result;
                for (const auto &item : data.as_array())
                {
                    result.push_back(Dividend(item));
                }
                return result;
            }

            Type type() const
            {
                return m_type;
            }

            std::string date_pay() const
            {
                return m_date_pay;
            }

            std::string date_ex() const
            {
                return m_date_ex;
            }

            double value() const
            {
                return m_value;
            }

        private:
            Type m_type;
            std::string m_date_pay;
            std::string m_date_ex;
            double m_value;
        };

        std::vector<Dividend> dividends(std::optional<std::string> &&html)
        {
            if (!html)
            {
                return {};
            }

            htmlParserCtxtPtr ctxt;
            impl::Dividends context;
            ctxt = htmlCreatePushParserCtxt(&impl::sax_handler, &context, "", 0, "", XML_CHAR_ENCODING_NONE);
            htmlParseChunk(ctxt, html->c_str(), html->size(), 0);
            htmlParseChunk(ctxt, "", 0, 1);
            htmlFreeParserCtxt(ctxt);

            return Dividend::load(context.json_parser.release());
        }
    } // namespace parsers
} // namespace html

namespace std
{
    std::ostream &operator<<(std::ostream &stream, const std::vector<html::parsers::Dividend> &dividends)
    {
        const auto width = 38;
        std::fill_n(std::ostream_iterator<char>(stream), width, '-');
        stream << std::endl;
        for (const auto &dividend : dividends)
        {
            stream << "|";
            if (dividend.type() == html::parsers::Dividend::Type::jcp)
            {
                stream << "JCP       | ";
            }
            else
            {
                stream << "Dividendo | ";
            }
            stream << dividend.date_pay() << " | " << std::setw(10) << dividend.value() << " |" << std::endl;
        }
        std::fill_n(std::ostream_iterator<char>(stream), width, '-');
        stream << std::endl;
        return stream;
    }
} // namespace std

int main()
{
    for (const auto ticker : {"enbr3", "itsa4"})
    {
        const auto results = html::parsers::dividends(request::get("https://statusinvest.com.br/acoes/" + std::string(ticker)));

        std::cout << "[" << ticker << "]" << std::endl;
        std::cout << results << std::endl;
    }

    return 0;
}
