#ifndef HEADER_GUARD_DPSG_IDENTITY_TYPE_HPP
#define HEADER_GUARD_DPSG_IDENTITY_TYPE_HPP

namespace dpsg::meta {
  template<class T>
    struct identity {
      using type = T;
    };
}

#endif // HEADER_GUARD_DPSG_IDENTITY_TYPE_HPP
